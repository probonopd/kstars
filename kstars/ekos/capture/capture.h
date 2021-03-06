/*  Ekos Capture tool
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "ui_capture.h"
#include "customproperties.h"
#include "oal/filter.h"
#include "ekos/ekos.h"
#include "indi/indiccd.h"
#include "indi/indicap.h"
#include "indi/indidome.h"
#include "indi/indilightbox.h"
#include "indi/inditelescope.h"
#include "ekos/auxiliary/filtermanager.h"

#include <QTimer>
#include <QUrl>
#include <QtDBus/QtDBus>

#include <memory>

class QProgressIndicator;
class QTableWidgetItem;
class KDirWatch;
class RotatorSettings;

/**
 * @namespace Ekos
 * @short Ekos is an advanced Astrophotography tool for Linux.
 * It is based on a modular extensible framework to perform common astrophotography tasks. This includes highly accurate GOTOs using astrometry solver, ability to measure and correct polar alignment errors ,
 * auto-focus & auto-guide capabilities, and capture of single or stack of images with filter wheel support.\n
 * Features:
 * - Control your telescope, CCD (& DSLRs), filter wheel, focuser, guider, adaptive optics unit, and any INDI-compatible auxiliary device from Ekos.
 * - Extremely accurate GOTOs using astrometry.net solver (both Online and Offline solvers supported).
 * - Load & Slew: Load a FITS image, slew to solved coordinates, and center the mount on the exact image coordinates in order to get the same desired frame.
 * - Measure & Correct Polar Alignment errors using astromety.net solver.
 * - Auto and manual focus modes using Half-Flux-Radius (HFR) method.
 * - Automated unattended meridian flip. Ekos performs post meridian flip alignment, calibration, and guiding to resume the capture session.
 * - Automatic focus between exposures when a user-configurable HFR limit is exceeded.
 * - Auto guiding with support for automatic dithering between exposures and support for Adaptive Optics devices in addition to traditional guiders.
 * - Powerful sequence queue for batch capture of images with optional prefixes, timestamps, filter wheel selection, and much more!
 * - Export and import sequence queue sets as Ekos Sequence Queue (.esq) files.
 * - Center the telescope anywhere in a captured FITS image or any FITS with World Coordinate System (WCS) header.
 * - Automatic flat field capture, just set the desired ADU and let Ekos does the rest!
 * - Automatic abort and resumption of exposure tasks if guiding errors exceed a user-configurable value.
 * - Support for dome slaving.
 * - Complete integration with KStars Observation Planner and SkyMap
 * - Integrate with all INDI native devices.
 * - Powerful scripting capabilities via \ref EkosDBusInterface "DBus."
 *
 * The primary class is EkosManager. It handles startup and shutdown of local and remote INDI devices, manages and orchesterates the various Ekos modules, and provides advanced DBus
 * interface to enable unattended scripting.
 *
 * @author Jasem Mutlaq
 * @version 1.5
 */
namespace Ekos
{
class SequenceJob;

/**
 *@class Capture
 *@short Captures single or sequence of images from a CCD.
 * The capture class support capturing single or multiple images from a CCD, it provides a powerful sequence queue with filter wheel selection. Any sequence queue can be saved as Ekos Sequence Queue (.esq).
 * All image capture operations are saved as Sequence Jobs that encapsulate all the different options in a capture process. The user may select in sequence autofocusing by setting a maximum HFR limit. When the limit
 * is exceeded, it automatically trigger autofocus operation. The capture process can also be linked with guide module. If guiding deviations exceed a certain threshold, the capture operation aborts until
 * the guiding deviation resume to acceptable levels and the capture operation is resumed.
 *@author Jasem Mutlaq
 *@version 1.3
 */
class Capture : public QWidget, public Ui::Capture
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Capture")

  public:
    typedef enum { MF_NONE, MF_INITIATED, MF_FLIPPING, MF_SLEWING, MF_ALIGNING, MF_GUIDING } MFStage;
    typedef enum {
        CAL_NONE,
        CAL_DUSTCAP_PARKING,
        CAL_DUSTCAP_PARKED,
        CAL_LIGHTBOX_ON,
        CAL_SLEWING,
        CAL_SLEWING_COMPLETE,
        CAL_MOUNT_PARKING,
        CAL_MOUNT_PARKED,
        CAL_DOME_PARKING,
        CAL_DOME_PARKED,
        CAL_PRECAPTURE_COMPLETE,
        CAL_CALIBRATION,
        CAL_CALIBRATION_COMPLETE,
        CAL_CAPTURING,
        CAL_DUSTCAP_UNPARKING,
        CAL_DUSTCAP_UNPARKED
    } CalibrationStage;
    typedef bool (Capture::*PauseFunctionPointer)();

    Capture();
    ~Capture();

    /** @defgroup CaptureDBusInterface Ekos DBus Interface - Capture Module
         * Ekos::Capture interface provides advanced scripting capabilities to capture image sequences.
        */

    /*@{*/

    /** DBUS interface function.
         * select the CCD device from the available CCD drivers.
         * @param device The CCD device name
         */
    Q_SCRIPTABLE bool setCCD(const QString &device);

    /** DBUS interface function.
         * select the filter device from the available filter drivers. The filter device can be the same as the CCD driver if the filter functionality was embedded within the driver.
         * @param device The filter device name
         */
    Q_SCRIPTABLE bool setFilter(QString device, int filterSlot);

    /** DBUS interface function.
         * Aborts any current jobs and remove all sequence queue jobs.
         */
    Q_SCRIPTABLE Q_NOREPLY void clearSequenceQueue();

    /** DBUS interface function.
         * Returns the overall sequence queue status. If there are no jobs pending, it returns "Invalid". If all jobs are idle, it returns "Idle". If all jobs are complete, it returns "Complete". If one or more jobs are aborted
         * it returns "Aborted" unless it was temporarily aborted due to guiding deviations, then it would return "Suspended". If one or more jobs have errors, it returns "Error". If any jobs is under progress, returns "Running".
         */
    Q_SCRIPTABLE QString getSequenceQueueStatus();

    /** DBUS interface function.
         * Loads the Ekos Sequence Queue file in the Sequence Queue. Jobs are appended to existing jobs.
         * @param fileURL full URL of the filename
         */
    Q_SCRIPTABLE bool loadSequenceQueue(const QString &fileURL);

    /** DBUS interface function.
         * Sets target name. The target name shall be appended to the root directory specified by the user.
         * e.g. If root directory is /home/jasem and target is M31, then root directory becomes /home/jasem/M31
         * @param name Name of desired target
         */
    Q_SCRIPTABLE Q_NOREPLY void setTargetName(const QString &name) { targetName = name; }

    /** DBUS interface function.
         * Sets Observer name. Observer name is sent to INDI CCD driver to include it in the FITS header
         * @param name Full name of observer
         */
    Q_SCRIPTABLE Q_NOREPLY void setObservrName(const QString &name) { observerName = name; }

    /** DBUS interface function.
         * Enables or disables the maximum guiding deviation and sets its value.
         * @param enable If true, enable the guiding deviation check, otherwise, disable it.
         * @param if enable is true, it sets the maximum guiding deviation in arcsecs. If the value is exceeded, the capture operation is aborted until the value falls below the value threshold.
         */
    Q_SCRIPTABLE Q_NOREPLY void setMaximumGuidingDeviaiton(bool enable, double value);

    /** DBUS interface function.
         * Enables or disables the in sequence focus and sets Half-Flux-Radius (HFR) limit.
         * @param enable If true, enable the in sequence auto focus check, otherwise, disable it.
         * @param if enable is true, it sets HFR in pixels. After each exposure, the HFR is re-measured and if it exceeds the specified value, an autofocus operation will be commanded.
         */
    Q_SCRIPTABLE Q_NOREPLY void setInSequenceFocus(bool enable, double HFR);

    /** DBUS interface function.
         * Enable or Disable meridian flip, and sets its activation hour.
         * @param enable If true, meridian flip will be command after user-configurable number of hours.
         */
    Q_SCRIPTABLE Q_NOREPLY void setMeridianFlip(bool enable);

    /** DBUS interface function.
         * Sets meridian flip trigger hour.
         * @param hours Number of hours after the meridian at which the mount is commanded to flip.
         */
    Q_SCRIPTABLE Q_NOREPLY void setMeridianFlipHour(double hours);

    /** DBUS interface function.
         * Does the CCD has a cooler control (On/Off) ?
         */
    Q_SCRIPTABLE bool hasCoolerControl();

    /** DBUS interface function.
         * Set the CCD cooler ON/OFF
         *
         */
    Q_SCRIPTABLE bool setCoolerControl(bool enable);

    /** DBUS interface function.
         * @return Returns the percentage of completed captures in all active jobs
         */
    Q_SCRIPTABLE double getProgressPercentage();

    /** DBUS interface function.
         * @return Returns the number of jobs in the sequence queue.
         */
    Q_SCRIPTABLE int getJobCount() { return jobs.count(); }

    /** DBUS interface function.
         * @return Returns the number of pending uncompleted jobs in the sequence queue.
         */
    Q_SCRIPTABLE int getPendingJobCount();

    /** DBUS interface function.
         * @return Returns ID of current active job if any, or -1 if there are no active jobs.
         */
    Q_SCRIPTABLE int getActiveJobID();

    /** DBUS interface function.
         * @return Returns time left in seconds until active job is estimated to be complete.
         */
    Q_SCRIPTABLE int getActiveJobRemainingTime();

    /** DBUS interface function.
         * @return Returns overall time left in seconds until all jobs are estimated to be complete
         */
    Q_SCRIPTABLE int getOverallRemainingTime();

    /** DBUS interface function.
         * @param id job number. Job IDs start from 0 to N-1.
         * @return Returns the job state (Idle, In Progress, Error, Aborted, Complete)
         */
    Q_SCRIPTABLE QString getJobState(int id);

    /** DBUS interface function.
         * @param id job number. Job IDs start from 0 to N-1.
         * @return Returns The number of images completed capture in the job.
         */
    Q_SCRIPTABLE int getJobImageProgress(int id);

    /** DBUS interface function.
         * @param id job number. Job IDs start from 0 to N-1.
         * @return Returns the total number of images to capture in the job.
         */
    Q_SCRIPTABLE int getJobImageCount(int id);

    /** DBUS interface function.
         * @param id job number. Job IDs start from 0 to N-1.
         * @return Returns the number of seconds left in an exposure operation.
         */
    Q_SCRIPTABLE double getJobExposureProgress(int id);

    /** DBUS interface function.
         * @param id job number. Job IDs start from 0 to N-1.
         * @return Returns the total requested exposure duration in the job.
         */
    Q_SCRIPTABLE double getJobExposureDuration(int id);

    /** DBUS interface function.
         * Clear in-sequence focus settings. It sets the autofocus HFR to zero so that next autofocus value is remembered for the in-sequence focusing.
         */
    Q_SCRIPTABLE Q_NOREPLY void clearAutoFocusHFR();

    /** DBUS interface function.
         * Jobs will NOT be checked for progress against the file system and will be always assumed as new jobs.
         */
    Q_SCRIPTABLE Q_NOREPLY void ignoreSequenceHistory();

    /** @}*/

    void addCCD(ISD::GDInterface *newCCD);
    void addFilter(ISD::GDInterface *newFilter);
    void setDome(ISD::GDInterface *device) { dome = dynamic_cast<ISD::Dome *>(device); }
    void setDustCap(ISD::GDInterface *device) { dustCap = dynamic_cast<ISD::DustCap *>(device); }
    void setLightBox(ISD::GDInterface *device) { lightBox = dynamic_cast<ISD::LightBox *>(device); }
    void addGuideHead(ISD::GDInterface *newCCD);
    void syncFrameType(ISD::GDInterface *ccd);
    void setTelescope(ISD::GDInterface *newTelescope);
    void setRotator(ISD::GDInterface *newRotator);
    void setFilterManager(const QSharedPointer<FilterManager> &manager);
    void syncTelescopeInfo();
    void syncFilterInfo();

    void clearLog();
    QString getLogText() { return logText.join("\n"); }

    /* Capture */
    void updateSequencePrefix(const QString &newPrefix, const QString &dir);

  public slots:

    /** \addtogroup CaptureDBusInterface
         *  @{
         */

    /* Capture */
    /** DBUS interface function.
         * Starts the sequence queue capture procedure sequentially by starting all jobs that are either Idle or Aborted in order.
         */
    Q_SCRIPTABLE Q_NOREPLY void start();

    /** DBUS interface function.
         * Stop all jobs and set current job status to aborted if abort is set to true, otherwise status is idle until
         * sequence is resumed or restarted.
         * @param abort abort jobs if in progress
         */
    Q_SCRIPTABLE Q_NOREPLY void stop(bool abort = false);

    /** DBUS interface function.
         * Aborts all jobs. It simply calls stop(true)
         */
    Q_SCRIPTABLE Q_NOREPLY void abort() { stop(true); }

    /** @}*/

    /**
         * @brief captureOne Capture one preview image
         */
    void captureOne();

    /**
         * @brief captureImage Initiates image capture in the active job.
         */
    void captureImage();

    /**
         * @brief newFITS process new FITS data received from camera. Update status of active job and overall sequence.
         * @param bp pointer to blob contianing FITS data
         */
    void newFITS(IBLOB *bp);

    /**
         * @brief checkCCD Refreshes the CCD information in the capture module.
         * @param CCDNum The CCD index in the CCD combo box to select as the active CCD.
         */
    void checkCCD(int CCDNum = -1);

    /**
         * @brief checkFilter Refreshes the filter wheel information in the capture module.
         * @param filterNum The filter wheel index in the filter device combo box to set as the active filter.
         */
    void checkFilter(int filterNum = -1);

    /**
         * @brief processCCDNumber Process number properties arriving from CCD. Currently, only CCD and Guider frames are processed.
         * @param nvp pointer to number property.
         */
    void processCCDNumber(INumberVectorProperty *nvp);

    /**
         * @brief processTelescopeNumber Process number properties arriving from telescope for meridian flip purposes.
         * @param nvp pointer to number property.
         */
    void processTelescopeNumber(INumberVectorProperty *nvp);

    /**
         * @brief addJob Add a new job to the sequence queue given the settings in the GUI.
         * @param preview True if the job is a preview job, otherwise, it is added as a batch job.
         * @return True if job is added successfully, false otherwise.
         */
    bool addJob(bool preview = false);

    /**
         * @brief removeJob Remove a job from the currently selected row. If no row is selected, it remove the last job in the queue.
         */
    void removeJob();

    /**
         * @brief moveJobUp Move the job in the sequence queue one place up.
         */
    void moveJobUp();

    /**
         * @brief moveJobDown Move the job in the sequence queue one place down.
         */
    void moveJobDown();

    /**
         * @brief setGuideDeviation Set the guiding deviaiton as measured by the guiding module. Abort capture if deviation exceeds user value. Resume capture if capture was aborted and guiding deviations are below user value.
         * @param delta_ra Deviation in RA in arcsecs from the selected guide star.
         * @param delta_dec Deviation in DEC in arcsecs from the selected guide star.
         */
    void setGuideDeviation(double delta_ra, double delta_dec);

    /**
         * @brief resumeCapture Resume capture after dither and/or focusing processes are complete.
         */
    bool resumeCapture();

    /**
         * @brief updateCCDTemperature Update CCD temperature in capture module.
         * @param value Temperature in celcius.
         */
    void updateCCDTemperature(double value);

    /**
         * @brief setTemperature Set CCD temperature from the user GUI settings.
         */
    void setTemperature();

    /**
         * @brief preparePreCaptureActions Check if we need to update filter position or CCD temperature before starting capture process
         */
    void preparePreCaptureActions();

    // Pause Sequence Queue
    void pause();

    // Logs
    void appendLogText(const QString &);

    // Auto Focus
    void setFocusStatus(Ekos::FocusState state);
    void setHFR(double newHFR) { focusHFR = newHFR; }

    // Guide
    void setGuideStatus(Ekos::GuideState state);
    // Align
    void setAlignStatus(Ekos::AlignState state);
    void setAlignResults(double orientation, double ra, double de, double pixscale);
    // Update Mount module status
    void setMountStatus(ISD::Telescope::TelescopeStatus newState);

  private slots:

    /**
         * @brief setDirty Set dirty bit to indicate sequence queue file was modified and needs saving.
         */
    void setDirty();

    void toggleSequence();

    void checkFrameType(int index);
    void resetFrame();
    void setExposureProgress(ISD::CCDChip *tChip, double value, IPState state);
    void checkSeqBoundary(const QString &path);
    void saveFITSDirectory();
    void setDefaultCCD(QString ccd);
    void setNewRemoteFile(QString file);
    void setGuideChip(ISD::CCDChip *chip);

    // Sequence Queue
    void loadSequenceQueue();
    void saveSequenceQueue();
    void saveSequenceQueueAs();

    // Jobs
    void resetJobs();
    void editJob(QModelIndex i);
    void resetJobEdit();
    void executeJob();

    // Meridian Flip
    void checkMeridianFlipTimeout();
    //void checkAlignmentSlewComplete();

    // AutoGuide
    void checkGuideDeviationTimeout();

    // Auto Focus
    //void updateAutofocusStatus(bool status, double HFR);
    //void startPostFilterAutoFocus();

    // Timed refocus
    void startRefocusEveryNTimer();
    void restartRefocusEveryNTimer();
    int getRefocusEveryNTimerElapsedSec();

    // Flat field
    void openCalibrationDialog();
    IPState processPreCaptureCalibrationStage();
    bool processPostCaptureCalibrationStage();
    void updatePreCaptureCalibrationStatus();

    // Send image info
    void sendNewImage(QImage *image, ISD::CCDChip *myChip);

    // Capture
    bool setCaptureComplete();

    // post capture script
    void postScriptFinished(int exitCode);

    // Filter focus offset
    //void showFilterOffsetDialog();
    //void loadFilterOffsets();

    // Live Video Preview
    void toggleVideoStream(bool enable);
    void setVideoStreamEnabled(bool enabled);

    // Observer
    void showObserverDialog();

    // Active Job Prepare State
    void updatePrepareState(Ekos::CaptureState prepareState);

    // Rotator
    void updateRotatorNumber(INumberVectorProperty *nvp);

  signals:
    void newLog();
    void checkFocus(double);
    void suspendGuiding();
    void resumeGuiding();
    void meridianFlipStarted();
    void meridianFlipCompleted();
    void newStatus(Ekos::CaptureState status);
    void newImage(QImage *image, Ekos::SequenceJob *job);
    void newExposureProgress(Ekos::SequenceJob *job);
    void newFocusOffset(int16_t offset);

  private:
    void setBusy(bool enable);
    bool resumeSequence();
    bool startNextExposure();
    // reset = 0 --> Do not reset
    // reset = 1 --> Full reset
    // reset = 2 --> Only update limits if needed
    void updateFrameProperties(int reset = 0);
    void prepareJob(SequenceJob *job);
    void syncGUIToJob(SequenceJob *job);
    bool processJobInfo(XMLEle *root);
    void processJobCompletion();
    bool saveSequenceQueue(const QString &path);
    void constructPrefix(QString &imagePrefix);
    double setCurrentADU(double value);
    void llsq(QVector<double> x, QVector<double> y, double &a, double &b);

    // DSLR Info
    void addDSLRInfo(const QString &model, uint32_t maxW, uint32_t maxH, double pixelW, double pixelH);
    void cullToCameraLimits();
    bool isModelinDSLRInfo(const QString &model);

    /* Meridian Flip */
    bool checkMeridianFlip();
    void checkGuidingAfterFlip();
    double getCurrentHA();

    // Remaining Time in seconds
    int getJobRemainingTime(SequenceJob *job);

    void resetFrameToZero();

    /* Capture */
    double seqExpose { 0 };
    int seqTotalCount { 0 };
    int seqCurrentCount { 0 };
    int seqDelay { 0 };
    int retries { 0 };
    QTimer *seqTimer { nullptr };
    QString seqPrefix;
    int nextSequenceID { 0 };
    int seqFileCount { 0 };
    bool isBusy { false };

    bool useGuideHead { false };

    QString targetName;
    QString observerName;

    SequenceJob *activeJob { nullptr };

    QList<ISD::CCD *> CCDs;

    ISD::CCDChip *targetChip { nullptr };
    ISD::CCDChip *guideChip { nullptr };

    // They're generic GDInterface because they could be either ISD::CCD or ISD::Filter
    QList<ISD::GDInterface *> Filters;

    QList<SequenceJob *> jobs;

    ISD::Telescope *currentTelescope { nullptr };
    ISD::CCD *currentCCD { nullptr };
    ISD::GDInterface *currentFilter { nullptr };
    ISD::GDInterface *currentRotator { nullptr };
    ISD::DustCap *dustCap { nullptr };
    ISD::LightBox *lightBox { nullptr };
    ISD::Dome *dome { nullptr };

    QStringList logText;
    QUrl sequenceURL;
    bool mDirty { false };
    bool jobUnderEdit { false };
    int currentFilterPosition { -1 };
    QProgressIndicator *pi { nullptr };

    // Guide Deviation
    bool deviationDetected { false };
    bool spikeDetected { false };
    QTimer guideDeviationTimer;

    // Autofocus
    bool isInSequenceFocus { false };
    bool requiredAutoFocusStarted { false };
    bool firstAutoFocus { true };
    double focusHFR { 0 }; // HFR value as received from the Ekos focus module
    double fileHFR { 0 };  // HFR value as loaded from the sequence file

    // Refocus every N minutes
    bool isRefocus { false };
    int refocusEveryNMinutesValue { 0 };  // number of minutes between forced refocus
    QElapsedTimer refocusEveryNTimer; // used to determine when next force refocus should occur

    // Meridan flip
    double initialHA { 0 };
    //double initialRA { 0 };
    SkyPoint initialMountCoords;
    bool resumeAlignmentAfterFlip { false };
    bool resumeGuidingAfterFlip { false };
    MFStage meridianFlipStage { MF_NONE };

    // Flat field automation
    QVector<double> ExpRaw, ADURaw;
    double targetADU { 0 };
    double targetADUTolerance { 1000 };
    SkyPoint wallCoord;
    bool preMountPark { false };
    bool preDomePark { false };
    FlatFieldDuration flatFieldDuration { DURATION_MANUAL };
    FlatFieldSource flatFieldSource { SOURCE_MANUAL };
    CalibrationStage calibrationStage { CAL_NONE };
    bool dustCapLightEnabled { false };
    bool lightBoxLightEnabled { false };
    ISD::CCD::UploadMode rememberUploadMode { ISD::CCD::UPLOAD_CLIENT };

    QUrl dirPath;

    // Misc
    bool ignoreJobProgress { true };
    bool suspendGuideOnDownload { false };

    // State
    CaptureState state { CAPTURE_IDLE };
    FocusState focusState { FOCUS_IDLE };
    GuideState guideState { GUIDE_IDLE };
    AlignState alignState { ALIGN_IDLE };

    PauseFunctionPointer pauseFunction;

    // CCD Chip frame settings
    QMap<ISD::CCDChip *, QVariantMap> frameSettings;

    // Post capture script
    QProcess postCaptureScript;

    // Rotator Settings
    std::unique_ptr<RotatorSettings> rotatorSettings;

    // How many images to capture before dithering operation is executed?
    uint8_t ditherCounter { 0 };
    uint8_t inSequenceFocusCounter { 0 };

    std::unique_ptr<CustomProperties> customPropertiesDialog;

    // Filter Manager
    QSharedPointer<FilterManager> filterManager;

    // DSLR Infos
    QList<QMap<QString,QVariant>> DSLRInfos;
};
}
