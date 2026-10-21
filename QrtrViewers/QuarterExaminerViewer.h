// QuarterExaminerViewer.h
//
// A drop-in style replacement for SoQtExaminerViewer, built on top of the
// Quarter library (SIM::Coin3D::Quarter::QuarterWidget).
//
// Quarter provides the OpenGL canvas and mouse navigation.  This class adds
// the "viewer chrome" that SoQt users are used to:
//
//   * left strip  : Pick / View / Home / Set Home / View All / Seek /
//                   Perspective<->Orthographic buttons, plus the "Rotx" wheel
//   * bottom strip: the "Roty" thumbwheel
//   * right strip : the "Dolly" thumbwheel
//   * right-click popup menu (functions, draw style, transparency, headlight,
//     decoration toggle, preferences)
//   * a preferences dialog (seek time / distance / target, camera zoom,
//     background colour, headlight, transparency)
//   * animated seek, home position handling, camera type switching
//
// Usage (mirrors SoQtExaminerViewer):
//
//     QApplication app(argc, argv);
//     SIM::Coin3D::Quarter::Quarter::init();
//     {
//         QuarterExaminerViewer viewer;
//         viewer.setSceneGraph(root);
//         viewer.show();
//         viewer.viewAll();
//         app.exec();
//     }
//     SIM::Coin3D::Quarter::Quarter::clean();

#ifndef QUARTEREXAMINERVIEWER_H
#define QUARTEREXAMINERVIEWER_H

#include <QColor>
#include <QElapsedTimer>
#include <QPoint>
#include <QTimer>
#include <QWidget>

#include <Quarter/Quarter.h>
#include <Inventor/SbLinear.h>

class SoCamera;
class SoNode;
class SoSeparator;

class QLabel;
class QMouseEvent;
class QToolButton;
class ThumbWheel;

namespace SIM { namespace Coin3D { namespace Quarter { class QuarterWidget; } } }

class QuarterExaminerViewer : public QWidget
{
    Q_OBJECT

public:
    enum CameraType { Perspective, Orthographic };

    enum DrawStyle {
        AsIs,
        Wireframe,
        WireframeOverlay,
        HiddenLine,
        Points,
        BoundingBox
    };

    explicit QuarterExaminerViewer(QWidget* parent = nullptr);
    ~QuarterExaminerViewer() override;

    // ---- scene graph -------------------------------------------------------
    // The viewer references the scene.  If the scene contains no camera, a
    // perspective camera is created and the scene is viewed with viewAll().
    void     setSceneGraph(SoNode* scene);
    SoNode*  getSceneGraph() const { return m_scene; }
    SoCamera* getCamera() const { return m_camera; }

    // Access to the underlying Quarter widget for anything not wrapped here.
    SIM::Coin3D::Quarter::QuarterWidget* quarterWidget() const { return m_quarter; }

    // ---- state queries -----------------------------------------------------
    bool       isViewing() const;
    bool       isHeadlight() const;
    bool       isDecoration() const { return m_decoration; }
    CameraType cameraType() const;
    DrawStyle  drawStyle() const { return m_drawStyle; }
    QColor     backgroundColor() const { return m_background; }

    // ---- seek settings (see also Preferences dialog) -------------------------
    void   setSeekTime(double seconds) { m_seekTime = seconds; }
    double seekTime() const { return m_seekTime; }
    void   setSeekDistance(float percent) { m_seekDistance = percent; }
    float  seekDistance() const { return m_seekDistance; }
    void   setSeekToObject(bool on) { m_seekToObject = on; }
    bool   isSeekToObject() const { return m_seekToObject; }

    void setPopupMenuEnabled(bool on) { m_popupEnabled = on; }
    bool isPopupMenuEnabled() const { return m_popupEnabled; }

public slots:
    void viewAll();
    void saveHomePosition();
    void resetToHomePosition();
    void seek();                       // arms seek: next left click picks target

    void setViewing(bool viewing);     // true = View (navigate), false = Pick
    void setHeadlight(bool on);
    void setDecoration(bool on);
    void setCameraType(CameraType type);
    void toggleCameraType();
    void setDrawStyle(DrawStyle style);
    void setBackgroundColor(const QColor& color);

    void showPreferences();

signals:
    void viewingChanged(bool viewing);
    void cameraTypeChanged(QuarterExaminerViewer::CameraType type);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void showEvent(QShowEvent* ev) override;

private slots:
    void onRotXMoved(float delta);
    void onRotYMoved(float delta);
    void onDollyMoved(float delta);
    void onSeekTick();
    void syncModeButtons();
    void onFrameSwapped();             // frame-rate probe (QRTRVIEWER_FPS=1)

private:
    struct CameraState {
        SbVec3f    position{0, 0, 0};
        SbRotation orientation{SbVec3f(0, 0, 1), 0.0f};
        float      focalDistance = 5.0f;
        float      height = 2.0f;          // orthographic height
        float      heightAngle = 0.785f;   // perspective height angle (radians)
        bool       valid = false;
    };

    void buildDecoration();
    QToolButton* makeButton(int iconKind, const QString& tip, bool checkable);
    void updateCameraButtonIcon();

    void captureState(CameraState& s) const;
    void applyState(const CameraState& s);

    void orbit(const SbVec3f& cameraLocalAxis, float angle);
    void dolly(float delta);
    void pan(float dxPixels, float dyPixels);
    void updateCursor();

    // The widget that actually receives mouse events: the QuarterWidget itself
    // in newer Quarter versions, or its viewport() child when QuarterWidget is
    // a QGraphicsView (older versions).
    QWidget* canvasWidget() const;

    // Mouse navigation in View mode (handled in the event filter)
    enum NavMode { NavNone, NavRotate, NavPan, NavZoom };
    bool beginNavigation(QMouseEvent* e);
    bool updateNavigation(QMouseEvent* e);
    bool endNavigation(QMouseEvent* e);

    void seekAt(const QPoint& widgetPos);
    void stopSeek();

    void showPopupMenu(const QPoint& globalPos);

    // ---- members -----------------------------------------------------------
    SIM::Coin3D::Quarter::QuarterWidget* m_quarter = nullptr;

    SoSeparator* m_root = nullptr;     // [camera?] + user scene
    SoNode*      m_scene = nullptr;    // user scene (referenced)
    SoCamera*    m_camera = nullptr;
    bool         m_cameraCreated = false;
    bool         m_pendingInitialView = false;

    // decoration
    QWidget*     m_leftStrip = nullptr;
    QWidget*     m_rightStrip = nullptr;
    QWidget*     m_bottomStrip = nullptr;
    QToolButton* m_pickBtn = nullptr;
    QToolButton* m_viewBtn = nullptr;
    QToolButton* m_homeBtn = nullptr;
    QToolButton* m_setHomeBtn = nullptr;
    QToolButton* m_viewAllBtn = nullptr;
    QToolButton* m_seekBtn = nullptr;
    QToolButton* m_cameraBtn = nullptr;
    ThumbWheel*  m_rotXWheel = nullptr;
    ThumbWheel*  m_rotYWheel = nullptr;
    ThumbWheel*  m_dollyWheel = nullptr;

    // home / seek
    CameraState m_home;
    CameraState m_seekFrom;
    CameraState m_seekTo;
    QTimer        m_seekTimer;
    QElapsedTimer m_seekClock;
    bool   m_seekArmed = false;
    bool   m_swallowRelease = false;
    double m_seekTime = 2.0;
    float  m_seekDistance = 50.0f;     // % of current distance kept after seek
    bool   m_seekToObject = false;

    // Pick/View mode, tracked here (true = View: the mouse navigates the camera)
    bool             m_viewing = true;

    int              m_fpsFrames = 0;        // frame-rate probe
    QElapsedTimer    m_fpsClock;

    // mouse navigation state
    NavMode          m_navMode = NavNone;
    Qt::MouseButton  m_navButton = Qt::NoButton;
    QPoint           m_navLast;

    // misc state
    DrawStyle m_drawStyle = AsIs;
    QColor    m_background = QColor(0, 0, 0);
    bool      m_decoration = true;
    bool      m_popupEnabled = true;
};

#endif // QUARTEREXAMINERVIEWER_H
