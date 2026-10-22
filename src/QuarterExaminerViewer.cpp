// QuarterExaminerViewer.cpp

#include "QrtrViewers/QuarterExaminerViewer.h"
#include "QrtrViewers/ThumbWheel.h"

#include <Quarter/QuarterWidget.h>

#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoEventManager.h>
#include <Inventor/SoPath.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/SoRenderManager.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>

#include <QAbstractScrollArea>
#include <QActionGroup>
#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QCheckBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

using SIM::Coin3D::Quarter::QuarterWidget;

namespace {

const double kPi = 3.14159265358979323846;

enum IconKind {
    IK_Pick, IK_View, IK_Home, IK_SetHome,
    IK_ViewAll, IK_Seek, IK_Perspective, IK_Orthographic
};

bool isOrtho(const SoCamera* cam)
{
    return cam && cam->isOfType(SoOrthographicCamera::getClassTypeId());
}

// Compose "orient" followed by a world-space rotation "rot".
// Inventor's SbRotation multiplication order is easy to get backwards, so the
// result is verified numerically against the rotated test vector and the
// other order is used if necessary.
SbRotation composeWorld(const SbRotation& orient, const SbRotation& rot)
{
    SbVec3f probe(0.36f, 0.48f, -0.8f);
    SbVec3f a, expected, b;
    orient.multVec(probe, a);
    rot.multVec(a, expected);

    SbRotation cand = orient * rot;
    cand.multVec(probe, b);
    if ((b - expected).sqrLength() < 1e-5f)
        return cand;
    return rot * orient;
}

// Groups several camera field writes into ONE notification.  Without this every
// field assignment notifies the scene graph separately (orbit() writes two
// fields, a rotate drag calls it twice), and Quarter's render manager reacts to
// each camera notification.
class CameraEditGuard
{
public:
    explicit CameraEditGuard(SoCamera* cam) : m_cam(cam)
    {
        if (!m_cam)
            return;
        m_pos    = m_cam->position.enableNotify(false);
        m_orient = m_cam->orientation.enableNotify(false);
        m_focal  = m_cam->focalDistance.enableNotify(false);
        if (isOrtho(m_cam)) {
            m_height = &static_cast<SoOrthographicCamera*>(m_cam)->height;
            m_heightFlag = m_height->enableNotify(false);
        }
    }
    ~CameraEditGuard()
    {
        if (!m_cam)
            return;
        m_cam->position.enableNotify(m_pos);
        m_cam->orientation.enableNotify(m_orient);
        m_cam->focalDistance.enableNotify(m_focal);
        if (m_height)
            m_height->enableNotify(m_heightFlag);
        m_cam->orientation.touch();          // single notification
    }
    CameraEditGuard(const CameraEditGuard&) = delete;
    CameraEditGuard& operator=(const CameraEditGuard&) = delete;

private:
    SoCamera* m_cam;
    SbBool m_pos = TRUE, m_orient = TRUE, m_focal = TRUE, m_heightFlag = TRUE;
    SoSFFloat* m_height = nullptr;
};

QPoint mousePosition(const QMouseEvent* e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position().toPoint();
#else
    return e->pos();
#endif
}

// -----------------------------------------------------------------------------
// Procedurally drawn icons, so no resource files are required.
// -----------------------------------------------------------------------------
QIcon makeIcon(int kind)
{
    const int S = 32;
    QPixmap pm(S * 2, S * 2);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor ink(40, 40, 40);
    p.setPen(QPen(ink, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    switch (kind) {
    case IK_Pick: {
        QPolygonF arrow;
        arrow << QPointF(9, 4) << QPointF(9, 24) << QPointF(14, 19)
              << QPointF(18, 28) << QPointF(21, 26.5) << QPointF(17, 18)
              << QPointF(24, 18);
        p.setBrush(Qt::white);
        p.drawPolygon(arrow);
        break;
    }
    case IK_View: {
        p.setBrush(QColor(250, 222, 185));
        for (int i = 0; i < 4; ++i) {
            const double top = (i == 1 || i == 2) ? 4.5 : 7.0;
            p.drawRoundedRect(QRectF(9.0 + i * 3.7, top, 3.5, 16.0), 1.7, 1.7);
        }
        p.drawRoundedRect(QRectF(9, 15, 14.8, 12), 4, 4);
        p.drawEllipse(QRectF(4.0, 15.5, 8.0, 4.0));
        break;
    }
    case IK_Home:
    case IK_SetHome: {
        QPolygonF roof;
        roof << QPointF(16, 5) << QPointF(4, 16) << QPointF(28, 16);
        p.setBrush(QColor(200, 70, 60));
        p.drawPolygon(roof);
        p.setBrush(QColor(235, 225, 200));
        p.drawRect(QRectF(7.5, 16, 17, 11));
        p.setBrush(QColor(110, 80, 50));
        p.drawRect(QRectF(13.5, 20, 5, 7));
        if (kind == IK_SetHome) {
            p.setBrush(QColor(40, 110, 220));
            p.drawEllipse(QRectF(20, 3, 9, 9));
            p.setPen(QPen(Qt::white, 1.6));
            p.drawLine(QPointF(24.5, 5), QPointF(24.5, 10));
            p.drawLine(QPointF(22, 7.5), QPointF(27, 7.5));
        }
        break;
    }
    case IK_ViewAll: {
        p.setBrush(QColor(120, 170, 230));
        p.drawRect(QRectF(11, 11, 10, 10));
        const double m = 4, e = 28, l = 7;
        // four corner brackets
        p.drawLine(QPointF(m, m), QPointF(m + l, m));
        p.drawLine(QPointF(m, m), QPointF(m, m + l));
        p.drawLine(QPointF(e, m), QPointF(e - l, m));
        p.drawLine(QPointF(e, m), QPointF(e, m + l));
        p.drawLine(QPointF(m, e), QPointF(m + l, e));
        p.drawLine(QPointF(m, e), QPointF(m, e - l));
        p.drawLine(QPointF(e, e), QPointF(e - l, e));
        p.drawLine(QPointF(e, e), QPointF(e, e - l));
        break;
    }
    case IK_Seek: {
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(14, 14), 8.5, 8.5);
        p.drawLine(QPointF(14, 2.5), QPointF(14, 8));
        p.drawLine(QPointF(14, 20), QPointF(14, 25.5));
        p.drawLine(QPointF(2.5, 14), QPointF(8, 14));
        p.drawLine(QPointF(20, 14), QPointF(25.5, 14));
        QPolygonF ptr;
        ptr << QPointF(16, 16) << QPointF(16, 29) << QPointF(19.5, 25.5)
            << QPointF(22.5, 30) << QPointF(24.5, 28.5) << QPointF(21.5, 24.5)
            << QPointF(26, 24);
        p.setBrush(Qt::white);
        p.drawPolygon(ptr);
        break;
    }
    case IK_Perspective: {
        // box with converging edges
        p.setBrush(QColor(170, 200, 235));
        QPolygonF front;
        front << QPointF(5, 27) << QPointF(27, 27) << QPointF(27, 12)
              << QPointF(5, 12);
        QPolygonF back;
        back << QPointF(11, 6) << QPointF(21, 6) << QPointF(21, 10)
             << QPointF(11, 10);
        p.drawPolygon(back);
        p.drawPolygon(front);
        p.drawLine(QPointF(5, 12), QPointF(11, 6));
        p.drawLine(QPointF(27, 12), QPointF(21, 6));
        break;
    }
    case IK_Orthographic: {
        // parallel-projected box
        p.setBrush(QColor(170, 200, 235));
        QPolygonF back;
        back << QPointF(11, 5) << QPointF(28, 5) << QPointF(28, 21)
             << QPointF(11, 21);
        p.drawPolygon(back);
        QPolygonF front;
        front << QPointF(4, 12) << QPointF(21, 12) << QPointF(21, 28)
              << QPointF(4, 28);
        p.drawPolygon(front);
        p.drawLine(QPointF(4, 12), QPointF(11, 5));
        p.drawLine(QPointF(21, 12), QPointF(28, 5));
        p.drawLine(QPointF(21, 28), QPointF(28, 21));
        break;
    }
    default:
        break;
    }
    p.end();
    return QIcon(pm);
}

} // namespace

// =============================================================================
// Construction
// =============================================================================

QuarterExaminerViewer::QuarterExaminerViewer(QWidget* parent)
    : QWidget(parent)
{
    m_quarter = new QuarterWidget(this);
    m_quarter->setContextMenuEnabled(false);         // we supply our own menu
    m_quarter->setInteractionModeEnabled(true);      // allow Pick/View toggling
    m_quarter->setInteractionModeOn(false);          // start in View mode
    m_quarter->setBackgroundColor(m_background);
    m_quarter->setHeadlightEnabled(true);            // light follows the camera
    // Depending on the Quarter version, mouse events go to the widget itself
    // or to a viewport() child; filter both.
    m_quarter->installEventFilter(this);
    if (canvasWidget() != m_quarter)
        canvasWidget()->installEventFilter(this);
    m_quarter->setMinimumSize(200, 200);

    buildDecoration();

    m_seekTimer.setInterval(15);
    connect(&m_seekTimer, &QTimer::timeout, this, &QuarterExaminerViewer::onSeekTick);

    // Optional frame-rate probe:  QRTRVIEWER_FPS=1 ./yourprogram
    // (uses QOpenGLWidget::frameSwapped(); silently does nothing without it)
    if (qEnvironmentVariableIsSet("QRTRVIEWER_FPS")) {
        m_fpsClock.start();
        connect(m_quarter, SIGNAL(frameSwapped()), this, SLOT(onFrameSwapped()));
    }

    syncModeButtons();
    updateCameraButtonIcon();
    updateCursor();
    resize(640, 520);
}

QuarterExaminerViewer::~QuarterExaminerViewer()
{
    m_seekTimer.stop();
    if (m_quarter)
        m_quarter->setSceneGraph(nullptr);
    if (m_root) {
        m_root->unref();
        m_root = nullptr;
    }
}

QToolButton* QuarterExaminerViewer::makeButton(int iconKind, const QString& tip,
                                               bool checkable)
{
    auto* b = new QToolButton;
    b->setIcon(makeIcon(iconKind));
    b->setIconSize(QSize(26, 26));
    b->setFixedSize(34, 34);
    b->setToolTip(tip);
    b->setCheckable(checkable);
    b->setFocusPolicy(Qt::NoFocus);
    b->setAutoRaise(false);
    return b;
}

void QuarterExaminerViewer::buildDecoration()
{
    // ---- left strip: buttons + Rotx wheel ----------------------------------
    m_pickBtn    = makeButton(IK_Pick,    tr("Pick mode: interact with the scene (Esc toggles)"), true);
    m_viewBtn    = makeButton(IK_View,    tr("View mode: navigate the camera (Esc toggles)"), true);
    m_homeBtn    = makeButton(IK_Home,    tr("Home: return to the stored viewpoint"), false);
    m_setHomeBtn = makeButton(IK_SetHome, tr("Set Home: store the current viewpoint"), false);
    m_viewAllBtn = makeButton(IK_ViewAll, tr("View All: fit the whole scene in the window"), false);
    m_seekBtn    = makeButton(IK_Seek,    tr("Seek: click on an object to fly to it"), false);
    m_cameraBtn  = makeButton(IK_Perspective, tr("Switch between perspective and orthographic camera"), false);

    m_rotXWheel = new ThumbWheel(Qt::Vertical);
    m_rotXWheel->setToolTip(tr("Rotate around the horizontal axis"));
    auto* rotXLabel = new QLabel(tr("Rotx"));
    rotXLabel->setAlignment(Qt::AlignHCenter);

    m_leftStrip = new QWidget;
    auto* lv = new QVBoxLayout(m_leftStrip);
    lv->setContentsMargins(2, 2, 2, 2);
    lv->setSpacing(2);
    for (QToolButton* b : { m_pickBtn, m_viewBtn, m_homeBtn, m_setHomeBtn,
                            m_viewAllBtn, m_seekBtn, m_cameraBtn })
        lv->addWidget(b, 0, Qt::AlignHCenter);
    lv->addStretch(1);
    lv->addWidget(rotXLabel);
    lv->addWidget(m_rotXWheel, 0, Qt::AlignHCenter);

    // ---- right strip: Dolly wheel ------------------------------------------
    m_dollyWheel = new ThumbWheel(Qt::Vertical);
    m_dollyWheel->setToolTip(tr("Dolly (zoom) the camera"));
    auto* dollyLabel = new QLabel(tr("Dolly"));
    dollyLabel->setAlignment(Qt::AlignHCenter);

    m_rightStrip = new QWidget;
    auto* rv = new QVBoxLayout(m_rightStrip);
    rv->setContentsMargins(2, 2, 2, 2);
    rv->setSpacing(2);
    rv->addStretch(1);
    rv->addWidget(dollyLabel);
    rv->addWidget(m_dollyWheel, 0, Qt::AlignHCenter);

    // ---- bottom strip: Roty wheel ------------------------------------------
    m_rotYWheel = new ThumbWheel(Qt::Horizontal);
    m_rotYWheel->setToolTip(tr("Rotate around the vertical axis"));
    auto* rotYLabel = new QLabel(tr("Roty"));

    m_bottomStrip = new QWidget;
    auto* bh = new QHBoxLayout(m_bottomStrip);
    bh->setContentsMargins(4, 2, 2, 2);
    bh->setSpacing(6);
    bh->addWidget(rotYLabel);
    bh->addWidget(m_rotYWheel);
    bh->addStretch(1);

    // ---- assemble -----------------------------------------------------------
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(0);
    grid->addWidget(m_leftStrip,   0, 0);
    grid->addWidget(m_quarter,     0, 1);
    grid->addWidget(m_rightStrip,  0, 2);
    grid->addWidget(m_bottomStrip, 1, 0, 1, 3);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(0, 1);

    // ---- connections --------------------------------------------------------
    connect(m_pickBtn,    &QToolButton::clicked, this, [this] { setViewing(false); });
    connect(m_viewBtn,    &QToolButton::clicked, this, [this] { setViewing(true); });
    connect(m_homeBtn,    &QToolButton::clicked, this, &QuarterExaminerViewer::resetToHomePosition);
    connect(m_setHomeBtn, &QToolButton::clicked, this, &QuarterExaminerViewer::saveHomePosition);
    connect(m_viewAllBtn, &QToolButton::clicked, this, &QuarterExaminerViewer::viewAll);
    connect(m_seekBtn,    &QToolButton::clicked, this, &QuarterExaminerViewer::seek);
    connect(m_cameraBtn,  &QToolButton::clicked, this, &QuarterExaminerViewer::toggleCameraType);

    connect(m_rotXWheel,  &ThumbWheel::wheelMoved, this, &QuarterExaminerViewer::onRotXMoved);
    connect(m_rotYWheel,  &ThumbWheel::wheelMoved, this, &QuarterExaminerViewer::onRotYMoved);
    connect(m_dollyWheel, &ThumbWheel::wheelMoved, this, &QuarterExaminerViewer::onDollyMoved);
}

// =============================================================================
// Scene graph and camera
// =============================================================================

void QuarterExaminerViewer::setSceneGraph(SoNode* scene)
{
    stopSeek();

    if (m_root) {
        m_quarter->setSceneGraph(nullptr);
        m_root->unref();
        m_root = nullptr;
    }
    if (m_scene) {
        m_scene->unref();
        m_scene = nullptr;
    }
    m_camera = nullptr;
    m_cameraCreated = false;
    m_home.valid = false;

    if (!scene)
        return;

    m_scene = scene;
    m_scene->ref();

    m_root = new SoSeparator;
    m_root->ref();
    // m_root holds the camera, which changes on every mouse move.  A render
    // cache on a node whose child changes every frame is invalidated every
    // frame, so it can never be reused; worse, while it is being (re)built the
    // scene below cannot use its own, perfectly valid, cache.  The user's scene
    // keeps its own caching settings.
    m_root->renderCaching.setValue(SoSeparator::OFF);

    // Does the user's scene have a camera?
    {
        SoSearchAction sa;
        sa.setType(SoCamera::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.setSearchingAll(false);
        sa.apply(m_scene);
        if (sa.getPath() == nullptr) {
            m_root->addChild(new SoPerspectiveCamera);
            m_cameraCreated = true;
        }
    }
    m_root->addChild(m_scene);

    {
        SoSearchAction sa;
        sa.setType(SoCamera::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.setSearchingAll(false);
        sa.apply(m_root);
        if (sa.getPath())
            m_camera = static_cast<SoCamera*>(sa.getPath()->getTail());
    }

    m_quarter->setSceneGraph(m_root);

    // Quarter wraps the scene in its own root that also holds the headlight,
    // which follows the camera and therefore also changes on every frame.
    if (SoNode* top = m_quarter->getSoRenderManager()->getSceneGraph()) {
        if (top != m_root && top->isOfType(SoSeparator::getClassTypeId()))
            static_cast<SoSeparator*>(top)->renderCaching.setValue(SoSeparator::OFF);
    }
    if (m_camera) {
        m_quarter->getSoRenderManager()->setCamera(m_camera);
        m_quarter->getSoEventManager()->setCamera(m_camera);
    }

    if (m_cameraCreated) {
        // A camera we made ourselves must be fitted to the scene; the viewport
        // size is only valid once the widget is on screen.
        if (isVisible()) {
            viewAll();
            saveHomePosition();
        } else {
            m_pendingInitialView = true;
        }
    } else {
        saveHomePosition();
    }
    updateCameraButtonIcon();
}

void QuarterExaminerViewer::showEvent(QShowEvent* ev)
{
    QWidget::showEvent(ev);
    if (m_pendingInitialView) {
        m_pendingInitialView = false;
        QTimer::singleShot(0, this, [this] {
            viewAll();
            saveHomePosition();
        });
    }
}

void QuarterExaminerViewer::viewAll()
{
    // QuarterWidget::viewAll() only posts an event to Quarter's scxml navigation
    // system, which does nothing when that system is inactive.  Do it directly.
    if (!m_camera || !m_root)
        return;
    stopSeek();
    const SbViewportRegion vp = m_quarter->getSoRenderManager()->getViewportRegion();
    m_camera->viewAll(m_root, vp);
}

void QuarterExaminerViewer::captureState(CameraState& s) const
{
    if (!m_camera)
        return;
    s.position = m_camera->position.getValue();
    s.orientation = m_camera->orientation.getValue();
    s.focalDistance = m_camera->focalDistance.getValue();
    const float f = std::max(s.focalDistance, 1e-6f);
    if (isOrtho(m_camera)) {
        s.height = static_cast<SoOrthographicCamera*>(m_camera)->height.getValue();
        s.heightAngle = 2.0f * std::atan(s.height / (2.0f * f));
    } else {
        s.heightAngle = static_cast<SoPerspectiveCamera*>(m_camera)->heightAngle.getValue();
        s.height = 2.0f * f * std::tan(s.heightAngle * 0.5f);
    }
    s.valid = true;
}

void QuarterExaminerViewer::applyState(const CameraState& s)
{
    if (!m_camera || !s.valid)
        return;
    m_camera->position = s.position;
    m_camera->orientation = s.orientation;
    m_camera->focalDistance = s.focalDistance;
    if (isOrtho(m_camera))
        static_cast<SoOrthographicCamera*>(m_camera)->height = s.height;
    else
        static_cast<SoPerspectiveCamera*>(m_camera)->heightAngle = s.heightAngle;
}

void QuarterExaminerViewer::saveHomePosition()
{
    captureState(m_home);
}

void QuarterExaminerViewer::resetToHomePosition()
{
    stopSeek();
    applyState(m_home);
}

QuarterExaminerViewer::CameraType QuarterExaminerViewer::cameraType() const
{
    return isOrtho(m_camera) ? Orthographic : Perspective;
}

void QuarterExaminerViewer::setCameraType(CameraType type)
{
    if (!m_camera || !m_root || cameraType() == type)
        return;
    stopSeek();

    CameraState cur;
    captureState(cur);
    SoCamera* old = m_camera;

    SoSearchAction sa;
    sa.setNode(old);
    sa.setInterest(SoSearchAction::FIRST);
    sa.setSearchingAll(false);
    sa.apply(m_root);
    SoPath* path = sa.getPath();
    if (!path || path->getLength() < 2)
        return;
    SoNode* parentNode = path->getNodeFromTail(1);
    if (!parentNode->isOfType(SoGroup::getClassTypeId()))
        return;
    SoGroup* parent = static_cast<SoGroup*>(parentNode);

    SoCamera* nc = (type == Orthographic)
                       ? static_cast<SoCamera*>(new SoOrthographicCamera)
                       : static_cast<SoCamera*>(new SoPerspectiveCamera);
    nc->ref();
    nc->position = old->position.getValue();
    nc->orientation = old->orientation.getValue();
    nc->nearDistance = old->nearDistance.getValue();
    nc->farDistance = old->farDistance.getValue();
    nc->aspectRatio = old->aspectRatio.getValue();
    nc->focalDistance = old->focalDistance.getValue();
    nc->viewportMapping.setValue(old->viewportMapping.getValue());
    if (type == Orthographic)
        static_cast<SoOrthographicCamera*>(nc)->height = cur.height;
    else
        static_cast<SoPerspectiveCamera*>(nc)->heightAngle = cur.heightAngle;

    parent->replaceChild(old, nc);   // 'old' may be destroyed by this call
    m_camera = nc;
    nc->unref();                     // the scene graph now holds the reference

    m_quarter->getSoRenderManager()->setCamera(m_camera);
    m_quarter->getSoEventManager()->setCamera(m_camera);

    updateCameraButtonIcon();
    emit cameraTypeChanged(type);
}

void QuarterExaminerViewer::toggleCameraType()
{
    setCameraType(cameraType() == Perspective ? Orthographic : Perspective);
}

void QuarterExaminerViewer::updateCameraButtonIcon()
{
    // The icon shows the camera type that a click will switch to.
    const bool ortho = (cameraType() == Orthographic);
    m_cameraBtn->setIcon(makeIcon(ortho ? IK_Perspective : IK_Orthographic));
    m_cameraBtn->setToolTip(ortho ? tr("Switch to perspective camera")
                                  : tr("Switch to orthographic camera"));
}

// =============================================================================
// Mode / display settings
// =============================================================================

bool QuarterExaminerViewer::isViewing() const
{
    return m_viewing;
}

void QuarterExaminerViewer::setViewing(bool viewing)
{
    m_viewing = viewing;
    m_navMode = NavNone;
    m_navButton = Qt::NoButton;
    // Pick mode lets Quarter forward events to the scene graph.
    m_quarter->setInteractionModeOn(!viewing);
    syncModeButtons();
    updateCursor();
    emit viewingChanged(viewing);
}

// Arrow cursor in Pick mode, hand in View mode (like SoQt).
void QuarterExaminerViewer::updateCursor()
{
    if (!m_quarter)
        return;

    Qt::CursorShape shape;
    if (m_seekArmed)
        shape = Qt::CrossCursor;
    else if (!m_viewing)
        shape = Qt::ArrowCursor;
    else if (m_navMode == NavZoom)
        shape = Qt::SizeVerCursor;
    else if (m_navMode != NavNone)
        shape = Qt::ClosedHandCursor;
    else
        shape = Qt::OpenHandCursor;

    // setCursor() is a platform call (and stopSeek(), which runs on every wheel
    // move, ends up here), so skip it when nothing changed.
    QWidget* canvas = canvasWidget();
    if (m_quarter->cursor().shape() != shape)
        m_quarter->setCursor(shape);
    if (canvas != m_quarter && canvas->cursor().shape() != shape)
        canvas->setCursor(shape);
}

QWidget* QuarterExaminerViewer::canvasWidget() const
{
    // qobject_cast works at run time, so this compiles whether or not
    // QuarterWidget derives from QAbstractScrollArea (QGraphicsView).
    if (auto* area = qobject_cast<QAbstractScrollArea*>(
            static_cast<QObject*>(m_quarter)))
        return area->viewport();
    return m_quarter;
}

void QuarterExaminerViewer::onFrameSwapped()
{
    ++m_fpsFrames;
    const qint64 ms = m_fpsClock.elapsed();
    if (ms >= 1000) {
        qInfo("QuarterExaminerViewer: %.1f frames/s", m_fpsFrames * 1000.0 / double(ms));
        m_fpsFrames = 0;
        m_fpsClock.restart();
    }
}

void QuarterExaminerViewer::syncModeButtons()
{
    const bool viewing = isViewing();
    QSignalBlocker b1(m_viewBtn);
    QSignalBlocker b2(m_pickBtn);
    m_viewBtn->setChecked(viewing);
    m_pickBtn->setChecked(!viewing);
}

bool QuarterExaminerViewer::isHeadlight() const
{
    return m_quarter->headlightEnabled();
}

void QuarterExaminerViewer::setHeadlight(bool on)
{
    m_quarter->setHeadlightEnabled(on);
}

void QuarterExaminerViewer::setDecoration(bool on)
{
    m_decoration = on;
    m_leftStrip->setVisible(on);
    m_rightStrip->setVisible(on);
    m_bottomStrip->setVisible(on);
}

namespace {
struct TranspEntry {
    const char* name;                              // shown in menu / dialog
    QuarterExaminerViewer::TransparencyType type;  // this class
    QuarterWidget::TransparencyType quarter;       // Quarter
};
const TranspEntry kTransparency[] = {
    { "None",                QuarterExaminerViewer::NoTransparency,   QuarterWidget::NONE },
    { "Screen door",         QuarterExaminerViewer::ScreenDoor,       QuarterWidget::SCREEN_DOOR },
    { "Add",                 QuarterExaminerViewer::Add,              QuarterWidget::ADD },
    { "Delayed add",         QuarterExaminerViewer::DelayedAdd,       QuarterWidget::DELAYED_ADD },
    { "Sorted object add",   QuarterExaminerViewer::SortedObjectAdd,  QuarterWidget::SORTED_OBJECT_ADD },
    { "Blend",               QuarterExaminerViewer::Blend,            QuarterWidget::BLEND },
    { "Delayed blend",       QuarterExaminerViewer::DelayedBlend,     QuarterWidget::DELAYED_BLEND },
    { "Sorted object blend", QuarterExaminerViewer::SortedObjectBlend, QuarterWidget::SORTED_OBJECT_BLEND },
};
} // namespace

void QuarterExaminerViewer::setTransparencyType(TransparencyType type)
{
    for (const TranspEntry& e : kTransparency) {
        if (e.type == type) {
            m_transparency = type;
            m_quarter->setTransparencyType(e.quarter);
            return;
        }
    }
}

QuarterExaminerViewer::TransparencyType QuarterExaminerViewer::transparencyType() const
{
    // Ask Quarter, so the answer stays right if the QuarterWidget was changed
    // directly.  A Quarter type this class has no name for (a newer sorted-
    // triangle mode, say) reports the last value set through this class.
    const QuarterWidget::TransparencyType current = m_quarter->transparencyType();
    for (const TranspEntry& e : kTransparency)
        if (e.quarter == current)
            return e.type;
    return m_transparency;
}

void QuarterExaminerViewer::setDrawStyle(DrawStyle style)
{
    m_drawStyle = style;
    QuarterWidget::RenderMode mode = QuarterWidget::AS_IS;
    switch (style) {
    case AsIs:             mode = QuarterWidget::AS_IS; break;
    case Wireframe:        mode = QuarterWidget::WIREFRAME; break;
    case WireframeOverlay: mode = QuarterWidget::WIREFRAME_OVERLAY; break;
    case HiddenLine:       mode = QuarterWidget::HIDDEN_LINE; break;
    case Points:           mode = QuarterWidget::POINTS; break;
    case BoundingBox:      mode = QuarterWidget::BOUNDING_BOX; break;
    }
    m_quarter->setRenderMode(mode);
}

void QuarterExaminerViewer::setBackgroundColor(const QColor& color)
{
    m_background = color;
    m_quarter->setBackgroundColor(color);
}

// =============================================================================
// Thumbwheel handlers
// =============================================================================

// Camera orbits around its focal point.  A wheel delta rotates the *object*
// by +delta, which is the camera turning by -delta around the same axis.
void QuarterExaminerViewer::orbit(const SbVec3f& localAxis, float angle)
{
    if (!m_camera)
        return;
    const SbRotation orient = m_camera->orientation.getValue();
    const float focal = m_camera->focalDistance.getValue();

    SbVec3f dir;
    orient.multVec(SbVec3f(0, 0, -1), dir);
    const SbVec3f focalPoint = m_camera->position.getValue() + dir * focal;

    SbVec3f worldAxis;
    orient.multVec(localAxis, worldAxis);
    const SbRotation newOrient = composeWorld(orient, SbRotation(worldAxis, angle));

    newOrient.multVec(SbVec3f(0, 0, -1), dir);
    m_camera->orientation = newOrient;
    m_camera->position = focalPoint - dir * focal;
}

void QuarterExaminerViewer::dolly(float delta)
{
    if (!m_camera)
        return;
    const float mult = std::exp(delta * 0.5f);

    if (isOrtho(m_camera)) {
        auto* oc = static_cast<SoOrthographicCamera*>(m_camera);
        oc->height = std::max(1e-6f, oc->height.getValue() * mult);
        return;
    }
    const float focal = m_camera->focalDistance.getValue();
    const float newFocal = std::max(1e-4f, focal * mult);

    SbVec3f dir;
    m_camera->orientation.getValue().multVec(SbVec3f(0, 0, -1), dir);
    const SbVec3f focalPoint = m_camera->position.getValue() + dir * focal;
    m_camera->position = focalPoint - dir * newFocal;
    m_camera->focalDistance = newFocal;
}

// Move the camera in its own x/y plane so the scene follows the mouse.
void QuarterExaminerViewer::pan(float dxPixels, float dyPixels)
{
    if (!m_camera)
        return;
    const float focal = m_camera->focalDistance.getValue();
    float visibleHeight;
    if (isOrtho(m_camera))
        visibleHeight = static_cast<SoOrthographicCamera*>(m_camera)->height.getValue();
    else
        visibleHeight = 2.0f * focal *
            std::tan(static_cast<SoPerspectiveCamera*>(m_camera)->heightAngle.getValue() * 0.5f);

    const float unitsPerPixel = visibleHeight / float(std::max(1, m_quarter->height()));

    const SbRotation orient = m_camera->orientation.getValue();
    SbVec3f right, up;
    orient.multVec(SbVec3f(1, 0, 0), right);
    orient.multVec(SbVec3f(0, 1, 0), up);

    m_camera->position = m_camera->position.getValue()
                       - right * (dxPixels * unitsPerPixel)
                       + up * (dyPixels * unitsPerPixel);
}

// Mouse bindings in View mode (same as SoQtExaminerViewer):
//   left drag            rotate
//   middle drag          pan            ctrl + left drag    pan
//   ctrl + middle drag   zoom           shift + left drag   zoom
//   mouse wheel          zoom           right click         popup menu
bool QuarterExaminerViewer::beginNavigation(QMouseEvent* e)
{
    if (!m_camera || m_navMode != NavNone)
        return false;

    const Qt::KeyboardModifiers mods = e->modifiers();
    NavMode mode = NavNone;
    if (e->button() == Qt::LeftButton) {
        if (mods & Qt::ControlModifier)      mode = NavPan;
        else if (mods & Qt::ShiftModifier)   mode = NavZoom;
        else                                 mode = NavRotate;
    } else if (e->button() == Qt::MiddleButton) {
        mode = (mods & Qt::ControlModifier) ? NavZoom : NavPan;
    }
    if (mode == NavNone)
        return false;                       // e.g. right button: let it through

    stopSeek();                             // cancels animation / armed seek
    m_navMode = mode;
    m_navButton = e->button();
    m_navLast = mousePosition(e);
    updateCursor();
    return true;
}

bool QuarterExaminerViewer::updateNavigation(QMouseEvent* e)
{
    if (m_navMode == NavNone)
        return false;

    const QPoint p = mousePosition(e);
    const QPoint d = p - m_navLast;
    m_navLast = p;

    CameraEditGuard guard(m_camera);        // one notification per mouse move

    switch (m_navMode) {
    case NavRotate: {
        const float k = float(kPi) / float(std::max(1, std::min(m_quarter->width(),
                                                                m_quarter->height())));
        // dragging right/down turns the object right/down
        orbit(SbVec3f(0, 1, 0), -float(d.x()) * k);
        orbit(SbVec3f(1, 0, 0), -float(d.y()) * k);
        break;
    }
    case NavPan:
        pan(float(d.x()), float(d.y()));
        break;
    case NavZoom:
        dolly(float(d.y()) * 0.02f);        // drag up = closer
        break;
    default:
        break;
    }
    return true;
}

bool QuarterExaminerViewer::endNavigation(QMouseEvent* e)
{
    if (m_navMode == NavNone || e->button() != m_navButton)
        return false;
    m_navMode = NavNone;
    m_navButton = Qt::NoButton;
    updateCursor();
    return true;
}

void QuarterExaminerViewer::onRotXMoved(float delta) { stopSeek(); CameraEditGuard g(m_camera); orbit(SbVec3f(1, 0, 0), -delta); }
void QuarterExaminerViewer::onRotYMoved(float delta) { stopSeek(); CameraEditGuard g(m_camera); orbit(SbVec3f(0, 1, 0), -delta); }
void QuarterExaminerViewer::onDollyMoved(float delta) { stopSeek(); CameraEditGuard g(m_camera); dolly(delta); }

// =============================================================================
// Seek
// =============================================================================

void QuarterExaminerViewer::seek()
{
    if (!m_camera)
        return;
    m_seekArmed = true;
    updateCursor();
}

void QuarterExaminerViewer::stopSeek()
{
    m_seekTimer.stop();
    m_seekArmed = false;
    updateCursor();
}

void QuarterExaminerViewer::seekAt(const QPoint& pos)
{
    if (!m_camera || !m_root)
        return;

    SoRenderManager* rm = m_quarter->getSoRenderManager();
    const SbViewportRegion& vp = rm->getViewportRegion();
    const SbVec2s vpSize = vp.getViewportSizePixels();
    const qreal dpr = m_quarter->devicePixelRatioF();

    const short x = short(pos.x() * dpr);
    const short y = short(vpSize[1] - 1 - pos.y() * dpr);

    SoRayPickAction pick(vp);
    pick.setPoint(SbVec2s(x, y));
    pick.setRadius(4.0f);
    pick.apply(m_root);

    SoPickedPoint* pp = pick.getPickedPoint();
    if (!pp)
        return;                       // clicked on background: nothing to do

    SbVec3f hit = pp->getPoint();
    if (m_seekToObject && pp->getPath()) {
        SoGetBoundingBoxAction bba(vp);
        bba.apply(pp->getPath());
        const SbBox3f box = bba.getBoundingBox();
        if (!box.isEmpty())
            hit = box.getCenter();
    }

    captureState(m_seekFrom);
    m_seekTo = m_seekFrom;

    const SbVec3f camPos = m_seekFrom.position;
    SbVec3f toHit = hit - camPos;
    const float dist = toHit.length();
    if (dist < 1e-6f)
        return;
    toHit /= dist;

    SbVec3f curDir;
    m_seekFrom.orientation.multVec(SbVec3f(0, 0, -1), curDir);
    m_seekTo.orientation = composeWorld(m_seekFrom.orientation, SbRotation(curDir, toHit));

    const float keep = std::max(0.01f, m_seekDistance / 100.0f);
    if (isOrtho(m_camera)) {
        m_seekTo.position = hit - toHit * dist;
        m_seekTo.focalDistance = dist;
        m_seekTo.height = m_seekFrom.height * keep;
    } else {
        const float newDist = std::max(dist * keep, 1e-4f);
        m_seekTo.position = hit - toHit * newDist;
        m_seekTo.focalDistance = newDist;
    }
    m_seekTo.valid = true;

    if (m_seekTime <= 0.0) {
        applyState(m_seekTo);
        return;
    }
    m_seekClock.start();
    m_seekTimer.start();
}

void QuarterExaminerViewer::onSeekTick()
{
    const double t = std::min(1.0, m_seekClock.elapsed() / (m_seekTime * 1000.0));
    const float e = float(0.5 - 0.5 * std::cos(kPi * t));   // ease in/out

    CameraState s;
    s.valid = true;
    s.position = m_seekFrom.position + (m_seekTo.position - m_seekFrom.position) * e;
    s.orientation = SbRotation::slerp(m_seekFrom.orientation, m_seekTo.orientation, e);
    s.focalDistance = m_seekFrom.focalDistance + (m_seekTo.focalDistance - m_seekFrom.focalDistance) * e;
    s.height = m_seekFrom.height + (m_seekTo.height - m_seekFrom.height) * e;
    s.heightAngle = m_seekFrom.heightAngle + (m_seekTo.heightAngle - m_seekFrom.heightAngle) * e;
    applyState(s);

    if (t >= 1.0)
        m_seekTimer.stop();
}

// =============================================================================
// Event filter (seek click, right-click menu, Esc key sync)
// =============================================================================

bool QuarterExaminerViewer::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == m_quarter || obj == canvasWidget()) {
        switch (ev->type()) {
        case QEvent::ContextMenu:
            if (m_popupEnabled) {
                showPopupMenu(static_cast<QContextMenuEvent*>(ev)->globalPos());
                return true;
            }
            break;

        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(ev);
            if (m_seekArmed && me->button() == Qt::LeftButton) {
                m_seekArmed = false;
                updateCursor();
                m_swallowRelease = true;
                seekAt(mousePosition(me));
                return true;
            }
            if (isViewing() && beginNavigation(me))
                return true;
            break;
        }
        case QEvent::MouseMove: {
            if (m_navMode != NavNone && updateNavigation(static_cast<QMouseEvent*>(ev)))
                return true;
            // Quarter feeds every mouse event it receives (hover included) to
            // the scene graph through an SoHandleEventAction, which walks the
            // WHOLE graph.  SoQt never does that in viewing mode, so neither
            // do we: in View mode the mouse belongs to the viewer.
            if (m_viewing)
                return true;
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(ev);
            if (m_swallowRelease && me->button() == Qt::LeftButton) {
                m_swallowRelease = false;
                return true;
            }
            if (m_navMode != NavNone && endNavigation(me))
                return true;
            if (m_viewing && (me->button() == Qt::LeftButton ||
                              me->button() == Qt::MiddleButton))
                return true;          // stray release: don't forward to the scene
            break;
        }
        case QEvent::MouseButtonDblClick:
            if (isViewing()) {
                const Qt::MouseButton b = static_cast<QMouseEvent*>(ev)->button();
                if (b == Qt::LeftButton || b == Qt::MiddleButton)
                    return true;      // keep Quarter's own handlers out of View mode
            }
            break;
        case QEvent::Wheel:
            if (isViewing() && m_camera) {
                const float steps = float(static_cast<QWheelEvent*>(ev)->angleDelta().y()) / 120.0f;
                if (steps != 0.0f) {
                    stopSeek();
                    CameraEditGuard g(m_camera);
                    dolly(-steps * 0.25f);   // wheel up = zoom in
                }
                return true;
            }
            break;
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
            // Escape toggles Pick/View.  We handle it here and swallow the key
            // so Quarter's own toggle can never get out of step with ours.
            if (static_cast<QKeyEvent*>(ev)->key() == Qt::Key_Escape) {
                auto* ke = static_cast<QKeyEvent*>(ev);
                if (ev->type() == QEvent::KeyPress && !ke->isAutoRepeat())
                    setViewing(!m_viewing);
                return true;
            }
            break;

        default:
            break;
        }
    }
    return QWidget::eventFilter(obj, ev);
}

// =============================================================================
// Popup menu
// =============================================================================

void QuarterExaminerViewer::showPopupMenu(const QPoint& globalPos)
{
    QMenu menu(this);

    // --- Functions ---
    QMenu* fn = menu.addMenu(tr("Functions"));
    fn->addAction(tr("Home"),     this, &QuarterExaminerViewer::resetToHomePosition);
    fn->addAction(tr("Set Home"), this, &QuarterExaminerViewer::saveHomePosition);
    fn->addAction(tr("View All"), this, &QuarterExaminerViewer::viewAll);
    fn->addAction(tr("Seek"),     this, &QuarterExaminerViewer::seek);

    // --- Viewing toggle ---
    QAction* viewing = menu.addAction(tr("Viewing"));
    viewing->setCheckable(true);
    viewing->setChecked(isViewing());
    connect(viewing, &QAction::toggled, this, &QuarterExaminerViewer::setViewing);

    // --- Draw style ---
    QMenu* ds = menu.addMenu(tr("Draw Style"));
    {
        struct Entry { const char* name; DrawStyle style; };
        static const Entry entries[] = {
            { "As is",             AsIs },
            { "Wireframe",         Wireframe },
            { "Wireframe overlay", WireframeOverlay },
            { "Hidden line",       HiddenLine },
            { "Points",            Points },
            { "Bounding box",      BoundingBox },
        };
        auto* group = new QActionGroup(ds);
        for (const Entry& e : entries) {
            QAction* a = ds->addAction(QString::fromLatin1(e.name));
            a->setCheckable(true);
            a->setChecked(m_drawStyle == e.style);
            group->addAction(a);
            const DrawStyle s = e.style;
            connect(a, &QAction::triggered, this, [this, s] { setDrawStyle(s); });
        }
    }

    // --- Transparency ---
    QMenu* tr_ = menu.addMenu(tr("Transparency Type"));
    {
        auto* group = new QActionGroup(tr_);
        const TransparencyType current = transparencyType();
        for (const TranspEntry& e : kTransparency) {
            QAction* a = tr_->addAction(QString::fromLatin1(e.name));
            a->setCheckable(true);
            a->setChecked(current == e.type);
            group->addAction(a);
            const TransparencyType t = e.type;
            connect(a, &QAction::triggered, this, [this, t] { setTransparencyType(t); });
        }
    }

    // --- Camera type ---
    QAction* persp = menu.addAction(tr("Orthographic Camera"));
    persp->setCheckable(true);
    persp->setChecked(cameraType() == Orthographic);
    connect(persp, &QAction::triggered, this, &QuarterExaminerViewer::toggleCameraType);

    menu.addSeparator();

    QAction* head = menu.addAction(tr("Headlight"));
    head->setCheckable(true);
    head->setChecked(isHeadlight());
    connect(head, &QAction::toggled, this, &QuarterExaminerViewer::setHeadlight);

    QAction* deco = menu.addAction(tr("Decoration"));
    deco->setCheckable(true);
    deco->setChecked(m_decoration);
    connect(deco, &QAction::toggled, this, &QuarterExaminerViewer::setDecoration);

    menu.addSeparator();
    menu.addAction(tr("Preferences..."), this, &QuarterExaminerViewer::showPreferences);

    menu.exec(globalPos);
}

// =============================================================================
// Preferences dialog
// =============================================================================

void QuarterExaminerViewer::showPreferences()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Examiner Viewer Preferences"));
    auto* form = new QFormLayout(&dlg);

    // Seek time
    auto* seekTime = new QDoubleSpinBox;
    seekTime->setRange(0.0, 10.0);
    seekTime->setSingleStep(0.25);
    seekTime->setSuffix(tr(" s"));
    seekTime->setValue(m_seekTime);
    connect(seekTime, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) { m_seekTime = v; });
    form->addRow(tr("Seek animation time:"), seekTime);

    // Seek distance
    auto* seekDist = new QDoubleSpinBox;
    seekDist->setRange(1.0, 200.0);
    seekDist->setSuffix(tr(" %"));
    seekDist->setValue(m_seekDistance);
    connect(seekDist, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) { m_seekDistance = float(v); });
    form->addRow(tr("Seek distance (of current):"), seekDist);

    // Seek target
    auto* seekTo = new QComboBox;
    seekTo->addItems({ tr("Picked point"), tr("Object center") });
    seekTo->setCurrentIndex(m_seekToObject ? 1 : 0);
    connect(seekTo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int i) { m_seekToObject = (i == 1); });
    form->addRow(tr("Seek to:"), seekTo);

    // Camera zoom (perspective field of view)
    auto* zoom = new QSlider(Qt::Horizontal);
    zoom->setRange(1, 140);
    auto* zoomLabel = new QLabel;
    auto updateZoomLabel = [zoomLabel](int deg) { zoomLabel->setText(QString::number(deg) + QStringLiteral("\u00B0")); };
    if (m_camera && !isOrtho(m_camera)) {
        const float ha = static_cast<SoPerspectiveCamera*>(m_camera)->heightAngle.getValue();
        const int deg = int(std::lround(ha * 180.0 / kPi));
        zoom->setValue(std::clamp(deg, 1, 140));
        updateZoomLabel(zoom->value());
        connect(zoom, &QSlider::valueChanged, this, [this, updateZoomLabel](int d) {
            updateZoomLabel(d);
            if (m_camera && !isOrtho(m_camera))
                static_cast<SoPerspectiveCamera*>(m_camera)->heightAngle = float(d * kPi / 180.0);
        });
    } else {
        zoom->setEnabled(false);
        zoomLabel->setText(tr("n/a"));
    }
    auto* zoomRow = new QHBoxLayout;
    zoomRow->addWidget(zoom, 1);
    zoomRow->addWidget(zoomLabel);
    form->addRow(tr("Camera zoom (field of view):"), zoomRow);

    // Background colour
    auto* bgBtn = new QPushButton;
    auto paintBg = [bgBtn](const QColor& c) {
        bgBtn->setStyleSheet(QStringLiteral("background-color: %1;").arg(c.name()));
    };
    paintBg(m_background);
    connect(bgBtn, &QPushButton::clicked, this, [this, &dlg, paintBg] {
        const QColor c = QColorDialog::getColor(m_background, &dlg, tr("Background color"));
        if (c.isValid()) {
            setBackgroundColor(c);
            paintBg(c);
        }
    });
    form->addRow(tr("Background color:"), bgBtn);

    // Transparency
    auto* transp = new QComboBox;
    int curIdx = 0, idx = 0;
    const TransparencyType cur = transparencyType();
    for (const TranspEntry& e : kTransparency) {
        transp->addItem(QString::fromLatin1(e.name));
        if (e.type == cur)
            curIdx = idx;
        ++idx;
    }
    transp->setCurrentIndex(curIdx);
    connect(transp, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int i) { setTransparencyType(kTransparency[i].type); });
    form->addRow(tr("Transparency type:"), transp);

    // Headlight
    auto* head = new QCheckBox(tr("Enabled"));
    head->setChecked(isHeadlight());
    connect(head, &QCheckBox::toggled, this, &QuarterExaminerViewer::setHeadlight);
    form->addRow(tr("Headlight:"), head);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(box);

    dlg.exec();
}
