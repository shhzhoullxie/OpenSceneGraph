#include <osg/GLExtensions>
#include <osg/Node>
#include <osg/Geometry>
#include <osg/Notify>
#include <osg/MatrixTransform>
#include <osg/Texture2D>
#include <osg/Stencil>
#include <osg/ColorMask>
#include <osg/Depth>
#include <osg/Billboard>
#include <osg/Material>
#include <osg/Projection>
#include <osg/TextureCubeMap>
#include <osg/io_utils>

#include <osgGA/TrackballManipulator>
#include <osgGA/FlightManipulator>
#include <osgGA/DriveManipulator>
#include <osgGA/StateSetManipulator>

#include <osgUtil/SmoothingVisitor>

#include <osgDB/Registry>
#include <osgDB/ReadFile>

#include <osgViewer/Viewer>
#include <osgViewer/StatsHandler>
#include <osgViewer/HelpHandler>

using namespace osg;

osg::Node* createDistortionSubgraph(osg::Node* subgraph, const osg::Vec4& clearColour)
{
    osg::Group* distortionNode = new osg::Group;
    
    unsigned int tex_width = 1024;
    unsigned int tex_height = 1024;
    
    osg::Texture2D* texture = new osg::Texture2D;
    texture->setTextureSize(tex_width, tex_height);
    texture->setInternalFormat(GL_RGBA);
    texture->setFilter(osg::Texture2D::MIN_FILTER,osg::Texture2D::LINEAR);
    texture->setFilter(osg::Texture2D::MAG_FILTER,osg::Texture2D::LINEAR);
   
    // set up the render to texture camera.
    {
        osg::Camera* camera = new osg::Camera;

        // set clear the color and depth buffer
        camera->setClearColor(clearColour);
        camera->setClearMask(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        // just inherit the main cameras view
        camera->setReferenceFrame(osg::Transform::RELATIVE_RF);
        camera->setProjectionMatrix(osg::Matrixd::identity());
        camera->setViewMatrix(osg::Matrixd::identity());

        // set viewport
        camera->setViewport(0,0,tex_width,tex_height);

        // set the camera to render before the main camera.
        camera->setRenderOrder(osg::Camera::PRE_RENDER);

        // tell the camera to use OpenGL frame buffer object where supported.
        camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);

        // attach the texture and use it as the color buffer.
        camera->attach(osg::Camera::COLOR_BUFFER, texture);

        // add subgraph to render
        camera->addChild(subgraph);
        
        distortionNode->addChild(camera);
   }
    
    // set up the hud camera
    {
        // create the quad to visualize.
        osg::Geometry* polyGeom = new osg::Geometry();

        polyGeom->setSupportsDisplayList(false);

        osg::Vec3 origin(0.0f,0.0f,0.0f);
        osg::Vec3 xAxis(1.0f,0.0f,0.0f);
        osg::Vec3 yAxis(0.0f,1.0f,0.0f);
        osg::Vec3 zAxis(0.0f,0.0f,1.0f);
        float height = 1024.0f;
        float width = 1280.0f;
        int noSteps = 50;

        osg::Vec3Array* vertices = new osg::Vec3Array;
        osg::Vec2Array* texcoords = new osg::Vec2Array;
        osg::Vec4Array* colors = new osg::Vec4Array;

        osg::Vec3 bottom = origin;
        osg::Vec3 dx = xAxis*(width/((float)(noSteps-1)));
        osg::Vec3 dy = yAxis*(height/((float)(noSteps-1)));

        osg::Vec2 bottom_texcoord(0.0f,0.0f);
        osg::Vec2 dx_texcoord(1.0f/(float)(noSteps-1),0.0f);
        osg::Vec2 dy_texcoord(0.0f,1.0f/(float)(noSteps-1));

        osg::Vec3 cursor = bottom;
        osg::Vec2 texcoord = bottom_texcoord;
        int i,j;
        for(i=0;i<noSteps;++i)
        {
            osg::Vec3 cursor = bottom+dy*(float)i;
            osg::Vec2 texcoord = bottom_texcoord+dy_texcoord*(float)i;
            for(j=0;j<noSteps;++j)
            {
                vertices->push_back(cursor);
                texcoords->push_back(osg::Vec2((sin(texcoord.x()*osg::PI-osg::PI*0.5)+1.0f)*0.5f,(sin(texcoord.y()*osg::PI-osg::PI*0.5)+1.0f)*0.5f));
                colors->push_back(osg::Vec4(1.0f,1.0f,1.0f,1.0f));

                cursor += dx;
                texcoord += dx_texcoord;
            }
        }

        // pass the created vertex array to the points geometry object.
        polyGeom->setVertexArray(vertices);

        polyGeom->setColorArray(colors);
        polyGeom->setColorBinding(osg::Geometry::BIND_PER_VERTEX);

        polyGeom->setTexCoordArray(0,texcoords);


        for(i=0;i<noSteps-1;++i)
        {
            osg::DrawElementsUShort* elements = new osg::DrawElementsUShort(osg::PrimitiveSet::QUAD_STRIP);
            for(j=0;j<noSteps;++j)
            {
                elements->push_back(j+(i+1)*noSteps);
                elements->push_back(j+(i)*noSteps);
            }
            polyGeom->addPrimitiveSet(elements);
        }


        // new we need to add the texture to the Drawable, we do so by creating a 
        // StateSet to contain the Texture StateAttribute.
        osg::StateSet* stateset = polyGeom->getOrCreateStateSet();
        stateset->setTextureAttributeAndModes(0, texture,osg::StateAttribute::ON);
        stateset->setMode(GL_LIGHTING,osg::StateAttribute::OFF);

        osg::Geode* geode = new osg::Geode();
        geode->addDrawable(polyGeom);

        // set up the camera to render the textured quad
        osg::Camera* camera = new osg::Camera;

        // just inherit the main cameras view
        camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
        camera->setViewMatrix(osg::Matrix::identity());
        camera->setProjectionMatrixAsOrtho2D(0,1280,0,1024);

        // set the camera to render before the main camera.
        camera->setRenderOrder(osg::Camera::NESTED_RENDER);

        // add subgraph to render
        camera->addChild(geode);
        
        distortionNode->addChild(camera);
    }
    return distortionNode;
}

void setDomeFaces(osgViewer::Viewer& viewer, osg::ArgumentParser& /*arguments*/)
{
 
    osg::GraphicsContext::WindowingSystemInterface* wsi = osg::GraphicsContext::getWindowingSystemInterface();
    if (!wsi) 
    {
        osg::notify(osg::NOTICE)<<"Error, no WindowSystemInterface available, cannot create windows."<<std::endl;
        return;
    }

    unsigned int width, height;
    wsi->getScreenResolution(osg::GraphicsContext::ScreenIdentifier(0), width, height);

    osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
    traits->x = 0;
    traits->y = 0;
    traits->width = width;
    traits->height = height;
    traits->windowDecoration = true;
    traits->doubleBuffer = true;
    traits->sharedContext = 0;

    osg::ref_ptr<osg::GraphicsContext> gc = osg::GraphicsContext::createGraphicsContext(traits.get());
    if (!gc)
    {
        osg::notify(osg::NOTICE)<<"GraphicsWindow has not been created successfully."<<std::endl;
        return;
    }


    int center_x = width/2;
    int center_y = height/2;
    int camera_width = 256;
    int camera_height = 256;

    // front face
    {
        osg::ref_ptr<osg::Camera> camera = new osg::Camera;
        camera->setGraphicsContext(gc.get());
        camera->setViewport(new osg::Viewport(center_x-camera_width/2, center_y, camera_width, camera_height));
        GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
        camera->setDrawBuffer(buffer);
        camera->setReadBuffer(buffer);

        viewer.addSlave(camera.get(), osg::Matrixd(), osg::Matrixd());
    }
    
    // top face
    {
        osg::ref_ptr<osg::Camera> camera = new osg::Camera;
        camera->setGraphicsContext(gc.get());
        camera->setViewport(new osg::Viewport(center_x-camera_width/2, center_y+camera_height, camera_width, camera_height));
        GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
        camera->setDrawBuffer(buffer);
        camera->setReadBuffer(buffer);

        viewer.addSlave(camera.get(), osg::Matrixd(), osg::Matrixd::rotate(osg::inDegrees(-90.0f), 1.0,0.0,0.0));
    }

    // left face
    {
        osg::ref_ptr<osg::Camera> camera = new osg::Camera;
        camera->setGraphicsContext(gc.get());
        camera->setViewport(new osg::Viewport(center_x-camera_width*3/2, center_y, camera_width, camera_height));
        GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
        camera->setDrawBuffer(buffer);
        camera->setReadBuffer(buffer);

        viewer.addSlave(camera.get(), osg::Matrixd(), osg::Matrixd::rotate(osg::inDegrees(-90.0f), 0.0,1.0,0.0));
    }

    // right face
    {
        osg::ref_ptr<osg::Camera> camera = new osg::Camera;
        camera->setGraphicsContext(gc.get());
        camera->setViewport(new osg::Viewport(center_x+camera_width/2, center_y, camera_width, camera_height));
        GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
        camera->setDrawBuffer(buffer);
        camera->setReadBuffer(buffer);

        viewer.addSlave(camera.get(), osg::Matrixd(), osg::Matrixd::rotate(osg::inDegrees(90.0f), 0.0,1.0,0.0));
    }

    // bottom face
    {
        osg::ref_ptr<osg::Camera> camera = new osg::Camera;
        camera->setGraphicsContext(gc.get());
        camera->setViewport(new osg::Viewport(center_x-camera_width/2, center_y-camera_height, camera_width, camera_height));
        GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
        camera->setDrawBuffer(buffer);
        camera->setReadBuffer(buffer);

        viewer.addSlave(camera.get(), osg::Matrixd(), osg::Matrixd::rotate(osg::inDegrees(90.0f), 1.0,0.0,0.0));
    }

    // back face
    {
        osg::ref_ptr<osg::Camera> camera = new osg::Camera;
        camera->setGraphicsContext(gc.get());
        camera->setViewport(new osg::Viewport(center_x-camera_width/2, center_y-2*camera_height, camera_width, camera_height));
        GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
        camera->setDrawBuffer(buffer);
        camera->setReadBuffer(buffer);

        viewer.addSlave(camera.get(), osg::Matrixd(), osg::Matrixd::rotate(osg::inDegrees(-180.0f), 1.0,0.0,0.0));
    }
    
    viewer.getCamera()->setProjectionMatrixAsPerspective(90.0f, 1.0, 1, 1000.0);

    viewer.assignSceneDataToCameras();
}

struct DrawCallback : public osg::Drawable::DrawCallback
{

    DrawCallback() {}

    virtual void drawImplementation(osg::State& state,const osg::Drawable* drawable) const
    {
        osg::notify(osg::NOTICE)<<"I am here"<<std::endl;
        osg::notify(osg::NOTICE)<<"  Projection matrix "<<state.getProjectionMatrix()<<std::endl;
        osg::notify(osg::NOTICE)<<"  ModelView matrix "<<state.getModelViewMatrix()<<std::endl;
        
        drawable->drawImplementation(state);
    }
};


void setDomeCorrection(osgViewer::Viewer& viewer, osg::ArgumentParser& /*arguments*/)
{
 
    osg::GraphicsContext::WindowingSystemInterface* wsi = osg::GraphicsContext::getWindowingSystemInterface();
    if (!wsi) 
    {
        osg::notify(osg::NOTICE)<<"Error, no WindowSystemInterface available, cannot create windows."<<std::endl;
        return;
    }

    unsigned int width, height;
    wsi->getScreenResolution(osg::GraphicsContext::ScreenIdentifier(0), width, height);

    osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
    traits->x = 0;
    traits->y = 0;
    traits->width = width;
    traits->height = height;
    traits->windowDecoration = false;
    traits->doubleBuffer = true;
    traits->sharedContext = 0;

    osg::ref_ptr<osg::GraphicsContext> gc = osg::GraphicsContext::createGraphicsContext(traits.get());
    if (!gc)
    {
        osg::notify(osg::NOTICE)<<"GraphicsWindow has not been created successfully."<<std::endl;
        return;
    }

    gc->setClearColor(osg::Vec4f(0.2f,0.2f,0.6f,1.0f));
    gc->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    int center_x = width/2;
    int center_y = height/2;
    int camera_width = 256;
    int camera_height = 256;
    
    int tex_width = 256;
    int tex_height = 256;
    
    osg::TextureCubeMap* texture = new osg::TextureCubeMap;
    texture->setTextureSize(tex_width, tex_height);
    texture->setInternalFormat(GL_RGB);
    texture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
    texture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
    texture->setWrap(osg::Texture::WRAP_R, osg::Texture::CLAMP_TO_EDGE);
    texture->setFilter(osg::TextureCubeMap::MIN_FILTER,osg::TextureCubeMap::LINEAR);
    texture->setFilter(osg::TextureCubeMap::MAG_FILTER,osg::TextureCubeMap::LINEAR);

    // front face
    {
        osg::ref_ptr<osg::Camera> camera = new osg::Camera;
        camera->setName("Front face camera");
        camera->setGraphicsContext(gc.get());
        camera->setViewport(new osg::Viewport(center_x-camera_width/2, center_y, camera_width, camera_height));
        GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
        camera->setDrawBuffer(buffer);
        camera->setReadBuffer(buffer);
        
        // attach the texture and use it as the color buffer.
        camera->attach(osg::Camera::COLOR_BUFFER, texture, 0, osg::TextureCubeMap::POSITIVE_Z);

        viewer.addSlave(camera.get(), osg::Matrixd(), osg::Matrixd());
    }
    
    // top face
    {
        osg::ref_ptr<osg::Camera> camera = new osg::Camera;
        camera->setName("Top face camera");
        camera->setGraphicsContext(gc.get());
        camera->setViewport(new osg::Viewport(center_x-camera_width/2, center_y+camera_height, camera_width, camera_height));
        GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
        camera->setDrawBuffer(buffer);
        camera->setReadBuffer(buffer);

        // attach the texture and use it as the color buffer.
        camera->attach(osg::Camera::COLOR_BUFFER, texture, 0, osg::TextureCubeMap::POSITIVE_Y);

        viewer.addSlave(camera.get(), osg::Matrixd(), osg::Matrixd::rotate(osg::inDegrees(-90.0f), 1.0,0.0,0.0));
    }

    // left face
    {
        osg::ref_ptr<osg::Camera> camera = new osg::Camera;
        camera->setName("Left face camera");
        camera->setGraphicsContext(gc.get());
        camera->setViewport(new osg::Viewport(center_x-camera_width*3/2, center_y, camera_width, camera_height));
        GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
        camera->setDrawBuffer(buffer);
        camera->setReadBuffer(buffer);

        // attach the texture and use it as the color buffer.
        camera->attach(osg::Camera::COLOR_BUFFER, texture, 0, osg::TextureCubeMap::NEGATIVE_X);

        viewer.addSlave(camera.get(), osg::Matrixd(), osg::Matrixd::rotate(osg::inDegrees(-90.0f), 0.0,1.0,0.0));
    }

    // right face
    {
        osg::ref_ptr<osg::Camera> camera = new osg::Camera;
        camera->setName("Right face camera");
        camera->setGraphicsContext(gc.get());
        camera->setViewport(new osg::Viewport(center_x+camera_width/2, center_y, camera_width, camera_height));
        GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
        camera->setDrawBuffer(buffer);
        camera->setReadBuffer(buffer);

        // attach the texture and use it as the color buffer.
        camera->attach(osg::Camera::COLOR_BUFFER, texture, 0, osg::TextureCubeMap::NEGATIVE_X);

        viewer.addSlave(camera.get(), osg::Matrixd(), osg::Matrixd::rotate(osg::inDegrees(90.0f), 0.0,1.0,0.0));
    }

    // bottom face
    {
        osg::ref_ptr<osg::Camera> camera = new osg::Camera;
        camera->setGraphicsContext(gc.get());
        camera->setName("Bottom face camera");
        camera->setViewport(new osg::Viewport(center_x-camera_width/2, center_y-camera_height, camera_width, camera_height));
        GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
        camera->setDrawBuffer(buffer);
        camera->setReadBuffer(buffer);

        // attach the texture and use it as the color buffer.
        camera->attach(osg::Camera::COLOR_BUFFER, texture, 0, osg::TextureCubeMap::NEGATIVE_Y);

        viewer.addSlave(camera.get(), osg::Matrixd(), osg::Matrixd::rotate(osg::inDegrees(90.0f), 1.0,0.0,0.0));
    }

    // back face
    {
        osg::ref_ptr<osg::Camera> camera = new osg::Camera;
        camera->setName("Back face camera");
        camera->setGraphicsContext(gc.get());
        camera->setViewport(new osg::Viewport(center_x-camera_width/2, center_y-2*camera_height, camera_width, camera_height));
        GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
        camera->setDrawBuffer(buffer);
        camera->setReadBuffer(buffer);

        // attach the texture and use it as the color buffer.
        camera->attach(osg::Camera::COLOR_BUFFER, texture, 0, osg::TextureCubeMap::NEGATIVE_Z);

        viewer.addSlave(camera.get(), osg::Matrixd(), osg::Matrixd::rotate(osg::inDegrees(-180.0f), 1.0,0.0,0.0));
    }
    
    viewer.getCamera()->setProjectionMatrixAsPerspective(90.0f, 1.0, 1, 1000.0);


    viewer.assignSceneDataToCameras();


    // distortion correction set up.
    {
        
        // create the quad to visualize.
        osg::Geometry* polyGeom = new osg::Geometry();

        polyGeom->setSupportsDisplayList(false);

        osg::Vec3 origin(0.0f,0.0f,0.0f);
        osg::Vec3 xAxis(1.0f,0.0f,0.0f);
        osg::Vec3 yAxis(0.0f,1.0f,0.0f);
        osg::Vec3 zAxis(0.0f,0.0f,1.0f);
        float height = 1024.0f;
        float width = 1280.0f;
        int noSteps = 50;

        osg::Vec3Array* vertices = new osg::Vec3Array;
        osg::Vec2Array* texcoords = new osg::Vec2Array;
        osg::Vec4Array* colors = new osg::Vec4Array;

        osg::Vec3 bottom = origin;
        osg::Vec3 dx = xAxis*(width/((float)(noSteps-1)));
        osg::Vec3 dy = yAxis*(height/((float)(noSteps-1)));

        osg::Vec2 bottom_texcoord(0.0f,0.0f);
        osg::Vec2 dx_texcoord(1.0f/(float)(noSteps-1),0.0f);
        osg::Vec2 dy_texcoord(0.0f,1.0f/(float)(noSteps-1));

        osg::Vec3 cursor = bottom;
        osg::Vec2 texcoord = bottom_texcoord;
        int i,j;
        for(i=0;i<noSteps;++i)
        {
            osg::Vec3 cursor = bottom+dy*(float)i;
            osg::Vec2 texcoord = bottom_texcoord+dy_texcoord*(float)i;
            for(j=0;j<noSteps;++j)
            {
                vertices->push_back(cursor);
                texcoords->push_back(osg::Vec2((sin(texcoord.x()*osg::PI-osg::PI*0.5)+1.0f)*0.5f,(sin(texcoord.y()*osg::PI-osg::PI*0.5)+1.0f)*0.5f));
                colors->push_back(osg::Vec4(1.0f,1.0f,1.0f,1.0f));

                cursor += dx;
                texcoord += dx_texcoord;
            }
        }

        // pass the created vertex array to the points geometry object.
        polyGeom->setVertexArray(vertices);

        polyGeom->setColorArray(colors);
        polyGeom->setColorBinding(osg::Geometry::BIND_PER_VERTEX);

        polyGeom->setTexCoordArray(0,texcoords);
        
        polyGeom->setDrawCallback(new DrawCallback);


        for(i=0;i<noSteps-1;++i)
        {
            osg::DrawElementsUShort* elements = new osg::DrawElementsUShort(osg::PrimitiveSet::QUAD_STRIP);
            for(j=0;j<noSteps;++j)
            {
                elements->push_back(j+(i+1)*noSteps);
                elements->push_back(j+(i)*noSteps);
            }
            polyGeom->addPrimitiveSet(elements);
        }


        // new we need to add the texture to the Drawable, we do so by creating a 
        // StateSet to contain the Texture StateAttribute.
        osg::StateSet* stateset = polyGeom->getOrCreateStateSet();
        stateset->setTextureAttributeAndModes(0, texture,osg::StateAttribute::ON);
        stateset->setMode(GL_LIGHTING,osg::StateAttribute::OFF);

        osg::Geode* geode = new osg::Geode();
        geode->addDrawable(polyGeom);

        osg::ref_ptr<osg::Camera> camera = new osg::Camera;
        camera->setGraphicsContext(gc.get());
        camera->setClearMask(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );
        camera->setClearColor( osg::Vec4(0.1,0.1,1.0,1.0) );
        camera->setViewport(new osg::Viewport(0, 0, width/2, height/2));
        GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
        camera->setDrawBuffer(buffer);
        camera->setReadBuffer(buffer);
        camera->setReferenceFrame(osg::Camera::ABSOLUTE_RF);
        //camera->setInheritanceMask(camera->getInheritanceMask() & ~osg::CullSettings::CLEAR_COLOR & ~osg::CullSettings::COMPUTE_NEAR_FAR_MODE);
        //camera->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
        
        camera->setProjectionMatrixAsOrtho2D(0,1280,0,1024);
        camera->setViewMatrix(osg::Matrix::identity());

        // add subgraph to render
        camera->addChild(geode);
        
        camera->setName("DistortionCorrectionCamera");

        osg::notify(osg::NOTICE)<<"Original Projection matrix "<<camera->getProjectionMatrix()<<std::endl;
        osg::notify(osg::NOTICE)<<"  View matrix "<<camera->getViewMatrix()<<std::endl;


        viewer.addSlave(camera.get(), osg::Matrixd(), osg::Matrixd());
    }
}


int main(int argc, char** argv)
{
    // use an ArgumentParser object to manage the program arguments.
    osg::ArgumentParser arguments(&argc,argv);

    // construct the viewer.
    osgViewer::Viewer viewer;
    
    // load the nodes from the commandline arguments.
    osg::Node* loadedModel = osgDB::readNodeFiles(arguments);
    if (!loadedModel)
    {
        osg::notify(osg::NOTICE)<<"Please specify a model filename on the command line."<<std::endl;
        return 1;
    }

    if (arguments.read("--dome"))
    {    
        viewer.setSceneData( loadedModel );

        setDomeCorrection(viewer, arguments);
    
    }
    else if (arguments.read("--faces"))
    {    
        viewer.setSceneData( loadedModel );

        setDomeFaces(viewer, arguments);
    
    }
    else
    {
        osg::Node* distortionNode = createDistortionSubgraph(loadedModel, viewer.getCamera()->getClearColor());

        // add model to the viewer.
        viewer.setSceneData( distortionNode );
    }

    viewer.setThreadingModel(osgViewer::Viewer::SingleThreaded);

    // add the state manipulator
    viewer.addEventHandler( new osgGA::StateSetManipulator(viewer.getCamera()->getOrCreateStateSet()) );
    
    // add the stats handler
    viewer.addEventHandler(new osgViewer::StatsHandler);

    return viewer.run();
}
;
