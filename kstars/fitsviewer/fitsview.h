/*  FITS Label
    Copyright (C) 2003-2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2016-2017 Robert Lancaster <rlancaste@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include "fitscommon.h"

#include <QFutureWatcher>
#include <QPixmap>
#include <QScrollArea>
#include <QStack>

#ifdef WIN32
// avoid compiler warning when windows.h is included after fitsio.h
#include <windows.h>
#endif

#include <fitsio.h>

#include <memory>

#define MINIMUM_PIXEL_RANGE 5
#define MINIMUM_STDVAR      5

class QAction;
class QEvent;
class QGestureEvent;
class QImage;
class QLabel;
class QPinchGesture;
class QResizeEvent;
class QToolBar;

class FITSData;
class FITSHistogram;
class FITSLabel;

class FITSView : public QScrollArea
{
    Q_OBJECT
  public:
    explicit FITSView(QWidget *parent = nullptr, FITSMode mode = FITS_NORMAL, FITSScale filter = FITS_NONE);
    ~FITSView();

    typedef enum {dragCursor, selectCursor, scopeCursor, crosshairCursor } CursorMode;

    // Loads FITS image, scales it, and displays it in the GUI
    bool loadFITS(const QString &filename, bool silent = true);
    // Save FITS
    int saveFITS(const QString &filename);
    // Rescale image lineary from image_buffer, fit to window if desired
    int rescale(FITSZoom type);

    // Access functions
    FITSData *getImageData() { return imageData; }
    double getCurrentZoom() { return currentZoom; }
    QImage *getDisplayImage() { return display_image; }

    // Tracking square
    void setTrackingBoxEnabled(bool enable);
    bool isTrackingBoxEnabled() { return trackingBoxEnabled; }
    QPixmap &getTrackingBoxPixmap();
    void setTrackingBox(const QRect &rect);
    const QRect &getTrackingBox() { return trackingBox; }

    // Overlay
    virtual void drawOverlay(QPainter *);

    // Overlay objects
    void drawStarCentroid(QPainter *);
    void drawTrackingBox(QPainter *);
    void drawMarker(QPainter *);
    void drawCrosshair(QPainter *);
    void drawEQGrid(QPainter *);
    void drawObjectNames(QPainter *painter);
    void drawPixelGrid(QPainter *painter);

    bool isCrosshairShown();
    bool areObjectsShown();
    bool isEQGridShown();
    bool isPixelGridShown();
    bool imageHasWCS();

    void updateFrame();

    bool isTelescopeActive();

    void enterEvent(QEvent *);
    void leaveEvent(QEvent *);
    CursorMode getCursorMode();
    void setCursorMode(CursorMode mode);
    void updateMouseCursor();

    void updateScopeButton();
    void setScopeButton(QAction *action) { centerTelescopeAction = action; }

    // Zoom related
    void cleanUpZoom(QPoint viewCenter);
    QPoint getImagePoint(QPoint viewPortPoint);

    // Star Detection
    int findStars(StarAlgorithm algorithm = ALGORITHM_CENTROID);
    void toggleStars(bool enable);
    void setStarsEnabled(bool enable);

    // FITS Mode
    void updateMode(FITSMode mode);
    FITSMode getMode() { return mode; }

    void setFilter(FITSScale newFilter) { filter = newFilter; }

    void setFirstLoad(bool value);

    void pushFilter(FITSScale value) { filterStack.push(value); }
    FITSScale popFilter() { return filterStack.pop(); }

    // Floating toolbar
    void createFloatingToolBar();

    //void setLoadWCSEnabled(bool value);

  public slots:
    void wheelEvent(QWheelEvent *event);
    void resizeEvent(QResizeEvent *event);
    void ZoomIn();
    void ZoomOut();
    void ZoomDefault();
    void ZoomToFit();

    // Grids
    void toggleEQGrid();
    void toggleObjects();
    void togglePixelGrid();
    void toggleCrosshair();

    // Stars
    void toggleStars();

    void centerTelescope();

    void processPointSelection(int x, int y);
    void processMarkerSelection(int x, int y);

  protected slots:
    /**
         * @brief syncWCSState Update toolbar and actions depending on whether WCS is available or not
         */
    void syncWCSState();
    //void handleWCSCompletion();

private:
    bool event(QEvent *event);
    bool gestureEvent(QGestureEvent *event);
    void pinchTriggered(QPinchGesture *gesture);

    template <typename T>
    int rescale(FITSZoom type);

    double average();
    double stddev();
    void calculateMaxPixel(double min, double max);
    void initDisplayImage();

    QPointF getPointForGridLabel();
    bool pointIsInImage(QPointF pt, bool scaled);

public:
    CursorMode lastMouseMode { selectCursor };

protected:
    /// WCS Future Watcher
    QFutureWatcher<bool> wcsWatcher;
    /// Cross hair
    QPointF markerCrosshair;
    /// Pointer to FITSData object
    FITSData *imageData { nullptr };
    /// Current zoom level
    double currentZoom { 0 };

private:
    QLabel *noImageLabel { nullptr };
    QPixmap noImage;

    QVector<QPointF> eqGridPoints;

    std::unique_ptr<FITSLabel> image_frame;

    int image_width { 0 };
    int image_height { 0 };

    /// Current width due to zoom
    uint16_t currentWidth { 0 };
    /// Current height due to zoom
    uint16_t currentHeight { 0 };
    /// Image zoom factor
    const double zoomFactor;

    /// FITS image that is displayed in the GUI
    QImage *display_image { nullptr };
    FITSHistogram *histogram { nullptr };

    bool firstLoad { true };
    bool markStars { false };
    bool showCrosshair { false };
    bool showObjects { false };
    bool showEQGrid { false };
    bool showPixelGrid { false };
    bool starsSearched { false };

    CursorMode cursorMode { selectCursor };
    bool zooming { false };
    int zoomTime { 0 };
    QPoint zoomLocation;

    QString filename;
    FITSMode mode;
    FITSScale filter;

    QStack<FITSScale> filterStack;

    // Star selection algorithm
    StarAlgorithm starAlgorithm { ALGORITHM_GRADIENT };

    // Tracking box
    bool trackingBoxEnabled { false };
    QRect trackingBox;
    QPixmap trackingBoxPixmap;

    // Scope pixmap
    QPixmap redScopePixmap;
    // Magenta Scope Pixmap
    QPixmap magentaScopePixmap;

    // Floating toolbar
    QToolBar *floatingToolBar { nullptr };
    QAction *centerTelescopeAction { nullptr };
    QAction *toggleEQGridAction { nullptr };
    QAction *toggleObjectsAction { nullptr };
    QAction *toggleStarsAction { nullptr };

  signals:
    void newStatus(const QString &msg, FITSBar id);
    void debayerToggled(bool);
    void wcsToggled(bool);
    void actionUpdated(const QString &name, bool enable);
    void trackingStarSelected(int x, int y);

    friend class FITSLabel;
};
