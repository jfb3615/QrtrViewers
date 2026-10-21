// main.cpp
//
// "Hello, Cone" from the Inventor Mentor (chapter 2), ported from
// SoQt + SoQtExaminerViewer to Qt + Quarter + QuarterExaminerViewer.

#include <QApplication>

#include <Quarter/Quarter.h>

#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSeparator.h>

#include "QrtrViewers/QuarterExaminerViewer.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    // Initializes Coin and Quarter.  Must come after QApplication is created.
    SIM::Coin3D::Quarter::Quarter::init();

    // --- Build the scene graph exactly like the Mentor example ---------------
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoMaterial* material = new SoMaterial;
    material->diffuseColor.setValue(1.0f, 0.0f, 0.0f);   // red
    root->addChild(material);

    root->addChild(new SoCone);

    // --- Show it in the examiner viewer ---------------------------------------
    {
        QuarterExaminerViewer viewer;
        viewer.setWindowTitle("Hello, Cone - Quarter Examiner Viewer");
        viewer.setSceneGraph(root);
        viewer.resize(640, 520);
        viewer.show();

        app.exec();
    }   // the viewer must be destroyed before Quarter::clean()

    root->unref();
    SIM::Coin3D::Quarter::Quarter::clean();
    return 0;
}
