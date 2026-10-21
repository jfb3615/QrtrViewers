#include <QApplication>
#include <QrtrViewers/QuarterExaminerViewer.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoVertexProperty.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCube.h>
#include <random>
#include <stdexcept>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <cmath>

SoSeparator *makeBranch() {
  // Make a separator:
  SoSeparator *sep=new SoSeparator;

  // Make it all turn red:
  SoMaterial *material=new SoMaterial;
  material->diffuseColor.setValue(1,0,0);
  sep->addChild(material);
  
  {
    // Make a draw style:
    SoDrawStyle *style=new SoDrawStyle;
    style->style=SoDrawStyle::LINES;
    sep->addChild(style);
    
    // Add a light model to make the lines more visible:
    SoLightModel *lightModel=new SoLightModel;
    lightModel->model=SoLightModel::BASE_COLOR;
    sep->addChild(lightModel);
  }
  
  // Add a cube:
  SoCube *cube=new SoCube;
  //sep->addChild(cube);

  // Add a scale factor:
  SoScale *scale = new SoScale;
  scale->scaleFactor.setValue(1.0/3.0, 1.0/3.0, 1.0/3.0);
  sep->addChild(scale);

  {
    // Make a draw style:
    SoDrawStyle *style=new SoDrawStyle;
    style->style=SoDrawStyle::FILLED;
    sep->addChild(style);
    
    // Add a light model to make the lines more visible:
    SoLightModel *lightModel=new SoLightModel;
    lightModel->model=SoLightModel::PHONG;
    sep->addChild(lightModel);

  }

  sep->addChild(cube);

  
  SoSeparator *subSep[3][3][3];
  for (int i=-1;i<2;i++) {
    for (int j=-1;j<2;j++) {
      for (int k=-1;k<2;k++) {
	subSep[i+1][j+1][k+1] = new SoSeparator;
	sep->addChild(subSep[i+1][j+1][k+1]);
	SoTranslation *trans=new SoTranslation;
	trans->translation.setValue(2*i,2*j,2*k);
	subSep[i+1][j+1][k+1]->addChild(trans);
      }
    }
  }				  
  return sep;
}

void replicate (SoSeparator *sep, int depth) {

  if (depth==0) return;

  for (int i=0;i<27;i++) {
    int N=sep->getNumChildren();
    SoSeparator *s=(SoSeparator *) sep->getChild(N-1-i);
    SoSeparator *branch=makeBranch();
    s->addChild(branch);
    replicate(branch,depth-1);
  }
}

int main(int argc, char ** argv)
{
 QApplication app(argc, argv);

 // Initializes Coin and Quarter.  Must come after QApplication is created.
 SIM::Coin3D::Quarter::Quarter::init();

  // The root of a scene graph
  SoSeparator *root = new SoSeparator;
  root->ref();

  SoSeparator *sep=makeBranch();
  root->addChild(sep);

  replicate(sep,3);

 // --- Show it in the examiner viewer ---------------------------------------
  {
    QuarterExaminerViewer viewer;
    viewer.setWindowTitle("Cuby");
    viewer.setSceneGraph(root);
    viewer.resize(640, 520);
    viewer.show();
    
    app.exec();
  }   // the viewer must be destroy
  
  
  root->unref();
  SIM::Coin3D::Quarter::Quarter::clean();

  return 0;
}
