/* Copyright (C) 2015 Google

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/


#include "BulletUrdfImporter.h"
#include "../../CommonInterfaces/CommonRenderInterface.h"
#include"../../ThirdPartyLibs/Wavefront/tiny_obj_loader.h"
#include "URDFImporterInterface.h"
#include "btBulletCollisionCommon.h"
#include "../ImportObjDemo/LoadMeshFromObj.h"
#include "../ImportSTLDemo/LoadMeshFromSTL.h"
#include "../ImportColladaDemo/LoadMeshFromCollada.h"
#include "BulletCollision/CollisionShapes/btShapeHull.h"//to create a tesselation of a generic btConvexShape
#include "../../CommonInterfaces/CommonGUIHelperInterface.h"
#include "Bullet3Common/b3FileUtils.h"
#include <string>
#include "../../Utils/b3ResourcePath.h"

#include "../ImportMeshUtility/b3ImportMeshUtility.h"

static btScalar gUrdfDefaultCollisionMargin = 0.001;

#include <iostream>
#include <fstream>
#include "UrdfParser.h"

struct MyTexture
{
	int m_width;
	int m_height;
	unsigned char* textureData;
};

struct BulletURDFInternalData
{
	UrdfParser m_urdfParser;
	struct GUIHelperInterface* m_guiHelper;
	char m_pathPrefix[1024];
	int m_bodyId;
	btHashMap<btHashInt,btVector4> m_linkColors;
    btAlignedObjectArray<btCollisionShape*> m_allocatedCollisionShapes;
	
	LinkVisualShapesConverter* m_customVisualShapesConverter;
};


void BulletURDFImporter::printTree()
{
//	btAssert(0);
}


enum MyFileType
{
	FILE_STL=1,
	FILE_COLLADA=2,
    FILE_OBJ=3,
};


    
BulletURDFImporter::BulletURDFImporter(struct GUIHelperInterface* helper, LinkVisualShapesConverter* customConverter)
{
	m_data = new BulletURDFInternalData;
	
	m_data->m_guiHelper = helper;
	m_data->m_pathPrefix[0]=0;
	m_data->m_customVisualShapesConverter = customConverter;

  
}

struct BulletErrorLogger : public ErrorLogger
{
	int m_numErrors;
	int m_numWarnings;
	
	BulletErrorLogger()
	:m_numErrors(0),
	m_numWarnings(0)
	{
	}
	virtual void reportError(const char* error)
	{
		m_numErrors++;
		b3Error(error);
	}
	virtual void reportWarning(const char* warning)
	{
		m_numWarnings++;
		b3Warning(warning);
	}
	
	virtual void printMessage(const char* msg)
	{
		b3Printf(msg);
	}
};	

bool BulletURDFImporter::loadURDF(const char* fileName, bool forceFixedBase)
{

	if (strlen(fileName)==0)
        return false;

//int argc=0;
	char relativeFileName[1024];
	
	b3FileUtils fu;
	
	//bool fileFound = fu.findFile(fileName, relativeFileName, 1024);
  	bool fileFound = b3ResourcePath::findResourcePath(fileName,relativeFileName,1024);
	
	std::string xml_string;
	m_data->m_pathPrefix[0] = 0;
    
    if (!fileFound){
        std::cerr << "URDF file not found" << std::endl;
		return false;
    } else
    {
		
		int maxPathLen = 1024;
		fu.extractPath(relativeFileName,m_data->m_pathPrefix,maxPathLen);


        std::fstream xml_file(relativeFileName, std::fstream::in);
        while ( xml_file.good())
        {
            std::string line;
            std::getline( xml_file, line);
            xml_string += (line + "\n");
        }
        xml_file.close();
    }

	BulletErrorLogger loggie;
    m_data->m_urdfParser.setParseSDF(false);
	bool result = m_data->m_urdfParser.loadUrdf(xml_string.c_str(), &loggie, forceFixedBase);

	return result;
}

int BulletURDFImporter::getNumModels() const
{
    return m_data->m_urdfParser.getNumModels();
}

void BulletURDFImporter::activateModel(int modelIndex)
{
    m_data->m_urdfParser.activateModel(modelIndex);
}


bool BulletURDFImporter::loadSDF(const char* fileName, bool forceFixedBase)
{
    
    //int argc=0;
    char relativeFileName[1024];
    
    b3FileUtils fu;
    
    //bool fileFound = fu.findFile(fileName, relativeFileName, 1024);
    bool fileFound = b3ResourcePath::findResourcePath(fileName,relativeFileName,1024);
    
    std::string xml_string;
    m_data->m_pathPrefix[0] = 0;
    
    if (!fileFound){
        std::cerr << "SDF file not found" << std::endl;
        return false;
    } else
    {
        
        int maxPathLen = 1024;
        fu.extractPath(relativeFileName,m_data->m_pathPrefix,maxPathLen);
        
        
        std::fstream xml_file(relativeFileName, std::fstream::in);
        while ( xml_file.good() )
        {
            std::string line;
            std::getline( xml_file, line);
            xml_string += (line + "\n");
        }
        xml_file.close();
    }
    
    BulletErrorLogger loggie;
    //todo: quick test to see if we can re-use the URDF parser for SDF or not
    m_data->m_urdfParser.setParseSDF(true);
    bool result = m_data->m_urdfParser.loadSDF(xml_string.c_str(), &loggie);
    
    return result;
}


const char* BulletURDFImporter::getPathPrefix()
{
	return m_data->m_pathPrefix;
}

    
void BulletURDFImporter::setBodyUniqueId(int bodyId)
{
    m_data->m_bodyId =bodyId;
}


int BulletURDFImporter::getBodyUniqueId() const
{
    return  m_data->m_bodyId;
}


BulletURDFImporter::~BulletURDFImporter()
{
	delete m_data;
}

    
int BulletURDFImporter::getRootLinkIndex() const
{
	if (m_data->m_urdfParser.getModel().m_rootLinks.size()==1)
	{
		return m_data->m_urdfParser.getModel().m_rootLinks[0]->m_linkIndex;
	}
    return -1;
};
    
void BulletURDFImporter::getLinkChildIndices(int linkIndex, btAlignedObjectArray<int>& childLinkIndices) const
{
	childLinkIndices.resize(0);
	UrdfLink* const* linkPtr = m_data->m_urdfParser.getModel().m_links.getAtIndex(linkIndex);
	if (linkPtr)
	{
		const UrdfLink* link = *linkPtr;
		//int numChildren = m_data->m_urdfParser->getModel().m_links.getAtIndex(linkIndex)->
		
		for (int i=0;i<link->m_childLinks.size();i++)
		{
			int childIndex =link->m_childLinks[i]->m_linkIndex;
			childLinkIndices.push_back(childIndex);
		}
	}
}


std::string BulletURDFImporter::getLinkName(int linkIndex) const
{
	UrdfLink* const* linkPtr = m_data->m_urdfParser.getModel().m_links.getAtIndex(linkIndex);
	btAssert(linkPtr);
	if (linkPtr)
	{
		UrdfLink* link = *linkPtr;
		return link->m_name;
	}
	return "";
}
    
std::string BulletURDFImporter::getJointName(int linkIndex) const
{
	UrdfLink* const* linkPtr = m_data->m_urdfParser.getModel().m_links.getAtIndex(linkIndex);
	btAssert(linkPtr);
	if (linkPtr)
	{
		UrdfLink* link = *linkPtr;
		if (link->m_parentJoint)
		{
			return link->m_parentJoint->m_name;
		}
	}
	return "";
}
    

void  BulletURDFImporter::getMassAndInertia(int linkIndex, btScalar& mass,btVector3& localInertiaDiagonal, btTransform& inertialFrame) const
{
	//todo(erwincoumans)
	//the link->m_inertia is NOT necessarily aligned with the inertial frame
	//so an additional transform might need to be computed
	UrdfLink* const* linkPtr = m_data->m_urdfParser.getModel().m_links.getAtIndex(linkIndex);
	
	
	btAssert(linkPtr);
	if (linkPtr)
	{
		UrdfLink* link = *linkPtr;
		btMatrix3x3 linkInertiaBasis;
		btScalar linkMass, principalInertiaX, principalInertiaY, principalInertiaZ;
		if (link->m_parentJoint==0 && m_data->m_urdfParser.getModel().m_overrideFixedBase)
		{
			linkMass = 0.f;
			principalInertiaX = 0.f;
			principalInertiaY = 0.f;
			principalInertiaZ = 0.f;
			linkInertiaBasis.setIdentity();
		}
		else
		{
			linkMass = link->m_inertia.m_mass;
			if (link->m_inertia.m_ixy == 0.0 &&
			    link->m_inertia.m_ixz == 0.0 &&
			    link->m_inertia.m_iyz == 0.0)
			{
				principalInertiaX = link->m_inertia.m_ixx;
				principalInertiaY = link->m_inertia.m_iyy;
				principalInertiaZ = link->m_inertia.m_izz;
				linkInertiaBasis.setIdentity();
			}
			else
			{
				principalInertiaX = link->m_inertia.m_ixx;
				btMatrix3x3 inertiaTensor(link->m_inertia.m_ixx, link->m_inertia.m_ixy, link->m_inertia.m_ixz,
							  link->m_inertia.m_ixy, link->m_inertia.m_iyy, link->m_inertia.m_iyz,
							  link->m_inertia.m_ixz, link->m_inertia.m_iyz, link->m_inertia.m_izz);
				btScalar threshold = 1.0e-6;
				int numIterations = 30;
				inertiaTensor.diagonalize(linkInertiaBasis, threshold, numIterations);
				principalInertiaX = inertiaTensor[0][0];
				principalInertiaY = inertiaTensor[1][1];
				principalInertiaZ = inertiaTensor[2][2];
			}
		}
		mass = linkMass;
		if (principalInertiaX < 0 ||
		    principalInertiaX > (principalInertiaY + principalInertiaZ) ||
		    principalInertiaY < 0 ||
		    principalInertiaY > (principalInertiaX + principalInertiaZ) ||
		    principalInertiaZ < 0 ||
		    principalInertiaZ > (principalInertiaX + principalInertiaY))
		{
			b3Warning("Bad inertia tensor properties, setting inertia to zero for link: %s\n", link->m_name.c_str());
			principalInertiaX = 0.f;
			principalInertiaY = 0.f;
			principalInertiaZ = 0.f;
			linkInertiaBasis.setIdentity();
		}
		localInertiaDiagonal.setValue(principalInertiaX, principalInertiaY, principalInertiaZ);
		inertialFrame.setOrigin(link->m_inertia.m_linkLocalFrame.getOrigin());
		inertialFrame.setBasis(link->m_inertia.m_linkLocalFrame.getBasis()*linkInertiaBasis);
	}
	else
	{
		mass = 1.f;
		localInertiaDiagonal.setValue(1,1,1);
		inertialFrame.setIdentity();
	}
}
    
bool BulletURDFImporter::getJointInfo(int urdfLinkIndex, btTransform& parent2joint, btTransform& linkTransformInWorld, btVector3& jointAxisInJointSpace, int& jointType, btScalar& jointLowerLimit, btScalar& jointUpperLimit, btScalar& jointDamping, btScalar& jointFriction) const
{
    jointLowerLimit = 0.f;
    jointUpperLimit = 0.f;
	jointDamping = 0.f;
	jointFriction = 0.f;

	UrdfLink* const* linkPtr = m_data->m_urdfParser.getModel().m_links.getAtIndex(urdfLinkIndex);
	btAssert(linkPtr);
	if (linkPtr)
	{
		UrdfLink* link = *linkPtr;
		linkTransformInWorld = link->m_linkTransformInWorld;
		
		if (link->m_parentJoint)
		{
			UrdfJoint* pj = link->m_parentJoint;
			parent2joint = pj->m_parentLinkToJointTransform;
			jointType = pj->m_type;
			jointAxisInJointSpace = pj->m_localJointAxis;
			jointLowerLimit = pj->m_lowerLimit;
			jointUpperLimit = pj->m_upperLimit;
			jointDamping = pj->m_jointDamping;
			jointFriction = pj->m_jointFriction;

			return true;
		} else
		{
			parent2joint.setIdentity();
			return false;
		}
	}
	
	return false;
	
}

bool BulletURDFImporter::getRootTransformInWorld(btTransform& rootTransformInWorld) const
{
    rootTransformInWorld = m_data->m_urdfParser.getModel().m_rootTransformInWorld;
    return true;
}

static btCollisionShape* createConvexHullFromShapes(std::vector<tinyobj::shape_t>& shapes, const btVector3& geomScale)
{
	btCompoundShape* compound = new btCompoundShape();
	compound->setMargin(gUrdfDefaultCollisionMargin);

	btTransform identity;
	identity.setIdentity();

	for (int s = 0; s<(int)shapes.size(); s++)
	{
		btConvexHullShape* convexHull = new btConvexHullShape();
		convexHull->setMargin(gUrdfDefaultCollisionMargin);
		tinyobj::shape_t& shape = shapes[s];
		int faceCount = shape.mesh.indices.size();

		for (int f = 0; f<faceCount; f += 3)
		{

			btVector3 pt;
			pt.setValue(shape.mesh.positions[shape.mesh.indices[f] * 3 + 0],
				shape.mesh.positions[shape.mesh.indices[f] * 3 + 1],
				shape.mesh.positions[shape.mesh.indices[f] * 3 + 2]);
			
			convexHull->addPoint(pt*geomScale,false);

			pt.setValue(shape.mesh.positions[shape.mesh.indices[f + 1] * 3 + 0],
						shape.mesh.positions[shape.mesh.indices[f + 1] * 3 + 1],
						shape.mesh.positions[shape.mesh.indices[f + 1] * 3 + 2]);
			convexHull->addPoint(pt*geomScale, false);

			pt.setValue(shape.mesh.positions[shape.mesh.indices[f + 2] * 3 + 0],
						shape.mesh.positions[shape.mesh.indices[f + 2] * 3 + 1],
						shape.mesh.positions[shape.mesh.indices[f + 2] * 3 + 2]);
			convexHull->addPoint(pt*geomScale, false);
		}

		convexHull->recalcLocalAabb();
		convexHull->optimizeConvexHull();
		compound->addChildShape(identity,convexHull);
	}

	return compound;
}

btCollisionShape* convertURDFToCollisionShape(const UrdfCollision* collision, const char* urdfPathPrefix)
{
	btCollisionShape* shape = 0;

    switch (collision->m_geometry.m_type)
    {
        case URDF_GEOM_CYLINDER:
        {
			btScalar cylRadius = collision->m_geometry.m_cylinderRadius;
			btScalar cylLength = collision->m_geometry.m_cylinderLength;
			
            btAlignedObjectArray<btVector3> vertices;
            //int numVerts = sizeof(barrel_vertices)/(9*sizeof(float));
            int numSteps = 32;
            for (int i=0;i<numSteps;i++)
            {

                btVector3 vert(cylRadius*btSin(SIMD_2_PI*(float(i)/numSteps)),cylRadius*btCos(SIMD_2_PI*(float(i)/numSteps)),cylLength/2.);
                vertices.push_back(vert);
                vert[2] = -cylLength/2.;
                vertices.push_back(vert);

            }
            btConvexHullShape* convexHull = new btConvexHullShape(&vertices[0].x(), vertices.size(), sizeof(btVector3));
            convexHull->setMargin(gUrdfDefaultCollisionMargin);
			convexHull->initializePolyhedralFeatures();
			convexHull->optimizeConvexHull();
			
			//btConvexShape* cylZShape = new btConeShapeZ(cyl->radius,cyl->length);//(vexHullShape(&vertices[0].x(), vertices.size(), sizeof(btVector3));
            
            //btVector3 halfExtents(cyl->radius,cyl->radius,cyl->length/2.);
            //btCylinderShapeZ* cylZShape = new btCylinderShapeZ(halfExtents);
            

            shape = convexHull;
            break;
        }
        case URDF_GEOM_BOX:
        {
			btVector3 extents = collision->m_geometry.m_boxSize;
			btBoxShape* boxShape = new btBoxShape(extents*0.5f);
			//btConvexShape* boxShape = new btConeShapeX(extents[2]*0.5,extents[0]*0.5);
            shape = boxShape;
			shape ->setMargin(gUrdfDefaultCollisionMargin);
            break;
        }
        case URDF_GEOM_SPHERE:
        {
            
			btScalar radius = collision->m_geometry.m_sphereRadius;
			btSphereShape* sphereShape = new btSphereShape(radius);
            shape = sphereShape;
			shape ->setMargin(gUrdfDefaultCollisionMargin);
            break;

            break;
        }
        case URDF_GEOM_MESH:
        {
			if (collision->m_name.length())
			{
				//b3Printf("collision->name=%s\n",collision->m_name.c_str());
			}
			if (1)
			{
				if (collision->m_geometry.m_meshFileName.length())
				{
					const char* filename = collision->m_geometry.m_meshFileName.c_str();
					//b3Printf("mesh->filename=%s\n",filename);
					char fullPath[1024];
					int fileType = 0;
					sprintf(fullPath,"%s%s",urdfPathPrefix,filename);
					b3FileUtils::toLower(fullPath);
                    char tmpPathPrefix[1024];
                    int maxPathLen = 1024;
                    b3FileUtils::extractPath(filename,tmpPathPrefix,maxPathLen);
                    
                    char collisionPathPrefix[1024];
                    sprintf(collisionPathPrefix,"%s%s",urdfPathPrefix,tmpPathPrefix);
                    
                    
                    
					if (strstr(fullPath,".dae"))
					{
						fileType = FILE_COLLADA;
					}
					if (strstr(fullPath,".stl"))
					{
						fileType = FILE_STL;
					}
                    if (strstr(fullPath,".obj"))
                   {
                       fileType = FILE_OBJ;
                   }

					sprintf(fullPath,"%s%s",urdfPathPrefix,filename);
					FILE* f = fopen(fullPath,"rb");
					if (f)
					{
						fclose(f);
						GLInstanceGraphicsShape* glmesh = 0;
						
						
						switch (fileType)
						{
                            case FILE_OBJ:
                            {
								if (collision->m_flags & URDF_FORCE_CONCAVE_TRIMESH)
								{
									glmesh = LoadMeshFromObj(fullPath, collisionPathPrefix);
								}
								else
								{
									std::vector<tinyobj::shape_t> shapes;
									std::string err = tinyobj::LoadObj(shapes, fullPath, collisionPathPrefix);
									//create a convex hull for each shape, and store it in a btCompoundShape

									shape = createConvexHullFromShapes(shapes, collision->m_geometry.m_meshScale);
									return shape;
								}
                                break;
                            }
						case FILE_STL:
							{
								glmesh = LoadMeshFromSTL(fullPath);
							break;
							}
						case FILE_COLLADA:
							{
								
								btAlignedObjectArray<GLInstanceGraphicsShape> visualShapes;
								btAlignedObjectArray<ColladaGraphicsInstance> visualShapeInstances;
								btTransform upAxisTrans;upAxisTrans.setIdentity();
								float unitMeterScaling=1;
								int upAxis = 2;
								LoadMeshFromCollada(fullPath,
													visualShapes, 
													visualShapeInstances,
													upAxisTrans,
													unitMeterScaling,
													upAxis );
								
								glmesh = new GLInstanceGraphicsShape;
						//		int index = 0;
								glmesh->m_indices = new b3AlignedObjectArray<int>();
								glmesh->m_vertices = new b3AlignedObjectArray<GLInstanceVertex>();

								for (int i=0;i<visualShapeInstances.size();i++)
								{
									ColladaGraphicsInstance* instance = &visualShapeInstances[i];
									GLInstanceGraphicsShape* gfxShape = &visualShapes[instance->m_shapeIndex];
		
									b3AlignedObjectArray<GLInstanceVertex> verts;
									verts.resize(gfxShape->m_vertices->size());

									int baseIndex = glmesh->m_vertices->size();

									for (int i=0;i<gfxShape->m_vertices->size();i++)
									{
										verts[i].normal[0] = 	gfxShape->m_vertices->at(i).normal[0];
										verts[i].normal[1] = 	gfxShape->m_vertices->at(i).normal[1];
										verts[i].normal[2] = 	gfxShape->m_vertices->at(i).normal[2];
										verts[i].uv[0] = gfxShape->m_vertices->at(i).uv[0];
										verts[i].uv[1] = gfxShape->m_vertices->at(i).uv[1];
										verts[i].xyzw[0] = gfxShape->m_vertices->at(i).xyzw[0];
										verts[i].xyzw[1] = gfxShape->m_vertices->at(i).xyzw[1];
										verts[i].xyzw[2] = gfxShape->m_vertices->at(i).xyzw[2];
										verts[i].xyzw[3] = gfxShape->m_vertices->at(i).xyzw[3];

									}

									int curNumIndices = glmesh->m_indices->size();
									int additionalIndices = gfxShape->m_indices->size();
									glmesh->m_indices->resize(curNumIndices+additionalIndices);
									for (int k=0;k<additionalIndices;k++)
									{
										glmesh->m_indices->at(curNumIndices+k)=gfxShape->m_indices->at(k)+baseIndex;
									}
			
									//compensate upAxisTrans and unitMeterScaling here
									btMatrix4x4 upAxisMat;
                                    upAxisMat.setIdentity();
									//upAxisMat.setPureRotation(upAxisTrans.getRotation());
									btMatrix4x4 unitMeterScalingMat;
									unitMeterScalingMat.setPureScaling(btVector3(unitMeterScaling,unitMeterScaling,unitMeterScaling));
									btMatrix4x4 worldMat = unitMeterScalingMat*instance->m_worldTransform*upAxisMat;
									//btMatrix4x4 worldMat = instance->m_worldTransform;
									int curNumVertices = glmesh->m_vertices->size();
									int additionalVertices = verts.size();
									glmesh->m_vertices->reserve(curNumVertices+additionalVertices);
									
									for(int v=0;v<verts.size();v++)
									{
										btVector3 pos(verts[v].xyzw[0],verts[v].xyzw[1],verts[v].xyzw[2]);
										pos = worldMat*pos;
										verts[v].xyzw[0] = float(pos[0]);
										verts[v].xyzw[1] = float(pos[1]);
										verts[v].xyzw[2] = float(pos[2]);
										glmesh->m_vertices->push_back(verts[v]);
									}
								}
								glmesh->m_numIndices = glmesh->m_indices->size();
								glmesh->m_numvertices = glmesh->m_vertices->size();
								//glmesh = LoadMeshFromCollada(fullPath);

								break;
							}
						default:
							{
                                b3Warning("Unsupported file type in Collision: %s\n",fullPath);
                                btAssert(0);
							}
						}
					

						if (glmesh && (glmesh->m_numvertices>0))
						{
							//b3Printf("extracted %d verticed from STL file %s\n", glmesh->m_numvertices,fullPath);
							//int shapeId = m_glApp->m_instancingRenderer->registerShape(&gvertices[0].pos[0],gvertices.size(),&indices[0],indices.size());
							//convex->setUserIndex(shapeId);
							btAlignedObjectArray<btVector3> convertedVerts;
							convertedVerts.reserve(glmesh->m_numvertices);
							for (int i=0;i<glmesh->m_numvertices;i++)
							{
								convertedVerts.push_back(btVector3(
                                           glmesh->m_vertices->at(i).xyzw[0]*collision->m_geometry.m_meshScale[0],
                                           glmesh->m_vertices->at(i).xyzw[1]*collision->m_geometry.m_meshScale[1],
                                           glmesh->m_vertices->at(i).xyzw[2]*collision->m_geometry.m_meshScale[2]));
							}
							
							if (collision->m_flags & URDF_FORCE_CONCAVE_TRIMESH)
							{
								btTriangleMesh* meshInterface = new btTriangleMesh();
								for (int i=0;i<glmesh->m_numIndices/3;i++)
								{
									float* v0 = glmesh->m_vertices->at(glmesh->m_indices->at(i*3)).xyzw;
									float* v1 = glmesh->m_vertices->at(glmesh->m_indices->at(i*3+1)).xyzw;
									float* v2 = glmesh->m_vertices->at(glmesh->m_indices->at(i*3+2)).xyzw;
									meshInterface->addTriangle(btVector3(v0[0],v0[1],v0[2]),
																btVector3(v1[0],v1[1],v1[2]),
															btVector3(v2[0],v2[1],v2[2]));
								}
								
								btBvhTriangleMeshShape* trimesh = new btBvhTriangleMeshShape(meshInterface,true,true);
								shape = trimesh;
							} else
							{
								btConvexHullShape* convexHull = new btConvexHullShape(&convertedVerts[0].getX(), convertedVerts.size(), sizeof(btVector3));
								convexHull->optimizeConvexHull();
								//convexHull->initializePolyhedralFeatures();
								convexHull->setMargin(gUrdfDefaultCollisionMargin);
								shape = convexHull;
							}
						} else
						{
							b3Warning("issue extracting mesh from STL file %s\n", fullPath);
						}

                        delete glmesh;
                       
					} else
					{
						b3Warning("mesh geometry not found %s\n",fullPath);
					}
							
				}
			}

					
            break;
        }
        default:
        {
            b3Warning("Error: unknown visual geometry type\n");
        }
    }
	return shape;
}


static void convertURDFToVisualShapeInternal(const UrdfVisual* visual, const char* urdfPathPrefix, const btTransform& visualTransform, btAlignedObjectArray<GLInstanceVertex>& verticesOut, btAlignedObjectArray<int>& indicesOut, btAlignedObjectArray<MyTexture>& texturesOut)
{

	
	GLInstanceGraphicsShape* glmesh = 0;

	btConvexShape* convexColShape = 0;

	switch (visual->m_geometry.m_type)
	{
		case URDF_GEOM_CYLINDER:
		{
			btAlignedObjectArray<btVector3> vertices;
		
			//int numVerts = sizeof(barrel_vertices)/(9*sizeof(float));
			int numSteps = 32;
			for (int i = 0; i<numSteps; i++)
			{

				btScalar cylRadius = visual->m_geometry.m_cylinderRadius;
				btScalar cylLength = visual->m_geometry.m_cylinderLength;
				
				btVector3 vert(cylRadius*btSin(SIMD_2_PI*(float(i) / numSteps)), cylRadius*btCos(SIMD_2_PI*(float(i) / numSteps)), cylLength / 2.);
				vertices.push_back(vert);
				vert[2] = -cylLength / 2.;
				vertices.push_back(vert);
			}

			btConvexHullShape* cylZShape = new btConvexHullShape(&vertices[0].x(), vertices.size(), sizeof(btVector3));
			cylZShape->setMargin(gUrdfDefaultCollisionMargin);
			convexColShape = cylZShape;
			break;
		}
		case URDF_GEOM_BOX:
		{
			
			btVector3 extents = visual->m_geometry.m_boxSize;
			
			btBoxShape* boxShape = new btBoxShape(extents*0.5f);
			//btConvexShape* boxShape = new btConeShapeX(extents[2]*0.5,extents[0]*0.5);
			convexColShape = boxShape;
			convexColShape->setMargin(gUrdfDefaultCollisionMargin);
			break;
		}
		case URDF_GEOM_SPHERE:
		{
			btScalar radius = visual->m_geometry.m_sphereRadius;
			btSphereShape* sphereShape = new btSphereShape(radius);
			convexColShape = sphereShape;
			convexColShape->setMargin(gUrdfDefaultCollisionMargin);
			break;

			break;
		}
		case URDF_GEOM_MESH:
		{
			if (visual->m_name.length())
			{
				//b3Printf("visual->name=%s\n", visual->m_name.c_str());
			}
			if (1)//visual->m_geometry)
			{
				if (visual->m_geometry.m_meshFileName.length())
				{
					const char* filename = visual->m_geometry.m_meshFileName.c_str();
					//b3Printf("mesh->filename=%s\n", filename);
					char fullPath[1024];
					int fileType = 0;
                    
                    char tmpPathPrefix[1024];
                    std::string xml_string;
                    int maxPathLen = 1024;
                    b3FileUtils::extractPath(filename,tmpPathPrefix,maxPathLen);
                   
                    char visualPathPrefix[1024];
                    sprintf(visualPathPrefix,"%s%s",urdfPathPrefix,tmpPathPrefix);
                    
                    
					sprintf(fullPath, "%s%s", urdfPathPrefix, filename);
					b3FileUtils::toLower(fullPath);
					if (strstr(fullPath, ".dae"))
					{
						fileType = FILE_COLLADA;
					}
					if (strstr(fullPath, ".stl"))
					{
						fileType = FILE_STL;
					}
                    if (strstr(fullPath,".obj"))
                    {
                        fileType = FILE_OBJ;
                    }


					sprintf(fullPath, "%s%s", urdfPathPrefix, filename);
					FILE* f = fopen(fullPath, "rb");
					if (f)
					{
						fclose(f);
						
						switch (fileType)
						{
                            case FILE_OBJ:
                            {
//                                glmesh = LoadMeshFromObj(fullPath,visualPathPrefix);
						
								b3ImportMeshData meshData;
								if (b3ImportMeshUtility::loadAndRegisterMeshFromFileInternal(fullPath, meshData))
								{
									
									if (meshData.m_textureImage)
									{
										MyTexture texData;
										texData.m_width = meshData.m_textureWidth;
										texData.m_height = meshData.m_textureHeight;
										texData.textureData = meshData.m_textureImage;
										texturesOut.push_back(texData);
									}
									glmesh = meshData.m_gfxShape;
								}
								
                                break;
                            }
                           
						case FILE_STL:
						{
							glmesh = LoadMeshFromSTL(fullPath);
							break;
						}
						case FILE_COLLADA:
						{

							btAlignedObjectArray<GLInstanceGraphicsShape> visualShapes;
							btAlignedObjectArray<ColladaGraphicsInstance> visualShapeInstances;
							btTransform upAxisTrans; upAxisTrans.setIdentity();
							float unitMeterScaling = 1;
							int upAxis = 2;

							LoadMeshFromCollada(fullPath,
								visualShapes,
								visualShapeInstances,
								upAxisTrans,
								unitMeterScaling,
												upAxis);

							glmesh = new GLInstanceGraphicsShape;
					//		int index = 0;
							glmesh->m_indices = new b3AlignedObjectArray<int>();
							glmesh->m_vertices = new b3AlignedObjectArray<GLInstanceVertex>();

							for (int i = 0; i<visualShapeInstances.size(); i++)
							{
								ColladaGraphicsInstance* instance = &visualShapeInstances[i];
								GLInstanceGraphicsShape* gfxShape = &visualShapes[instance->m_shapeIndex];

								b3AlignedObjectArray<GLInstanceVertex> verts;
								verts.resize(gfxShape->m_vertices->size());

								int baseIndex = glmesh->m_vertices->size();

								for (int i = 0; i<gfxShape->m_vertices->size(); i++)
								{
									verts[i].normal[0] = gfxShape->m_vertices->at(i).normal[0];
									verts[i].normal[1] = gfxShape->m_vertices->at(i).normal[1];
									verts[i].normal[2] = gfxShape->m_vertices->at(i).normal[2];
									verts[i].uv[0] = gfxShape->m_vertices->at(i).uv[0];
									verts[i].uv[1] = gfxShape->m_vertices->at(i).uv[1];
									verts[i].xyzw[0] = gfxShape->m_vertices->at(i).xyzw[0];
									verts[i].xyzw[1] = gfxShape->m_vertices->at(i).xyzw[1];
									verts[i].xyzw[2] = gfxShape->m_vertices->at(i).xyzw[2];
									verts[i].xyzw[3] = gfxShape->m_vertices->at(i).xyzw[3];

								}

								int curNumIndices = glmesh->m_indices->size();
								int additionalIndices = gfxShape->m_indices->size();
								glmesh->m_indices->resize(curNumIndices + additionalIndices);
								for (int k = 0; k<additionalIndices; k++)
								{
									glmesh->m_indices->at(curNumIndices + k) = gfxShape->m_indices->at(k) + baseIndex;
								}

								//compensate upAxisTrans and unitMeterScaling here
								btMatrix4x4 upAxisMat;
								upAxisMat.setIdentity();
//								upAxisMat.setPureRotation(upAxisTrans.getRotation());
								btMatrix4x4 unitMeterScalingMat;
								unitMeterScalingMat.setPureScaling(btVector3(unitMeterScaling, unitMeterScaling, unitMeterScaling));
								btMatrix4x4 worldMat = unitMeterScalingMat*upAxisMat*instance->m_worldTransform;
								//btMatrix4x4 worldMat = instance->m_worldTransform;
								int curNumVertices = glmesh->m_vertices->size();
								int additionalVertices = verts.size();
								glmesh->m_vertices->reserve(curNumVertices + additionalVertices);

								for (int v = 0; v<verts.size(); v++)
								{
									btVector3 pos(verts[v].xyzw[0], verts[v].xyzw[1], verts[v].xyzw[2]);
									pos = worldMat*pos;
									verts[v].xyzw[0] = float(pos[0]);
									verts[v].xyzw[1] = float(pos[1]);
									verts[v].xyzw[2] = float(pos[2]);
									glmesh->m_vertices->push_back(verts[v]);
								}
							}
							glmesh->m_numIndices = glmesh->m_indices->size();
							glmesh->m_numvertices = glmesh->m_vertices->size();
							//glmesh = LoadMeshFromCollada(fullPath);

							break;
						}
						default:
						{
                            b3Warning("Error: unsupported file type for Visual mesh: %s\n", fullPath);
                            btAssert(0);
						}
						}


						if (glmesh && glmesh->m_vertices && (glmesh->m_numvertices>0))
						{
						    //apply the geometry scaling
						    for (int i=0;i<glmesh->m_vertices->size();i++)
                            {
                                glmesh->m_vertices->at(i).xyzw[0] *= visual->m_geometry.m_meshScale[0];
                                glmesh->m_vertices->at(i).xyzw[1] *= visual->m_geometry.m_meshScale[1];
                                glmesh->m_vertices->at(i).xyzw[2] *= visual->m_geometry.m_meshScale[2];
                            }
						    
						}
						else
						{
							b3Warning("issue extracting mesh from COLLADA/STL file %s\n", fullPath);
						}

					}
					else
					{
						b3Warning("mesh geometry not found %s\n", fullPath);
					}


				}
			}


			break;
		}
		default:
		{
			b3Warning("Error: unknown visual geometry type\n");
		}
	}

	//if we have a convex, tesselate into localVertices/localIndices
	if ((glmesh==0) && convexColShape)
	{
		btShapeHull* hull = new btShapeHull(convexColShape);
		hull->buildHull(0.0);
		{
			//	int strideInBytes = 9*sizeof(float);
			int numVertices = hull->numVertices();
			int numIndices = hull->numIndices();

			
			glmesh = new GLInstanceGraphicsShape;
		//	int index = 0;
			glmesh->m_indices = new b3AlignedObjectArray<int>();
			glmesh->m_vertices = new b3AlignedObjectArray<GLInstanceVertex>();


			for (int i = 0; i < numVertices; i++)
			{
				GLInstanceVertex vtx;
				btVector3 pos = hull->getVertexPointer()[i];
				vtx.xyzw[0] = pos.x();
				vtx.xyzw[1] = pos.y();
				vtx.xyzw[2] = pos.z();
				vtx.xyzw[3] = 1.f;
				pos.normalize();
				vtx.normal[0] = pos.x();
				vtx.normal[1] = pos.y();
				vtx.normal[2] = pos.z();
				vtx.uv[0] = 0.5f;
				vtx.uv[1] = 0.5f;
				glmesh->m_vertices->push_back(vtx);
			}

			btAlignedObjectArray<int> indices;
			for (int i = 0; i < numIndices; i++)
			{
				glmesh->m_indices->push_back(hull->getIndexPointer()[i]);
			}
			
			glmesh->m_numvertices = glmesh->m_vertices->size();
			glmesh->m_numIndices = glmesh->m_indices->size();
		}
        delete hull;
		delete convexColShape;
		convexColShape = 0;

	}
	
	if (glmesh && glmesh->m_numIndices>0 && glmesh->m_numvertices >0)
	{

		int baseIndex = verticesOut.size();



		for (int i = 0; i < glmesh->m_indices->size(); i++)
		{
			indicesOut.push_back(glmesh->m_indices->at(i) + baseIndex);
		}

		for (int i = 0; i < glmesh->m_vertices->size(); i++)
		{
			GLInstanceVertex& v = glmesh->m_vertices->at(i);
			btVector3 vert(v.xyzw[0],v.xyzw[1],v.xyzw[2]);
			btVector3 vt = visualTransform*vert;
			v.xyzw[0] = vt[0];
			v.xyzw[1] = vt[1];
			v.xyzw[2] = vt[2];
			btVector3 triNormal(v.normal[0],v.normal[1],v.normal[2]);
			triNormal = visualTransform.getBasis()*triNormal;
			v.normal[0] = triNormal[0];
			v.normal[1] = triNormal[1];
			v.normal[2] = triNormal[2];
			verticesOut.push_back(v);
		}
	}
    delete glmesh;
    
}


int BulletURDFImporter::convertLinkVisualShapes(int linkIndex, const char* pathPrefix, const btTransform& localInertiaFrame) const
{
    int graphicsIndex = -1;
    btAlignedObjectArray<GLInstanceVertex> vertices;
	btAlignedObjectArray<int> indices;
	btTransform startTrans; startTrans.setIdentity();
	btAlignedObjectArray<MyTexture> textures;
	
    const UrdfModel& model = m_data->m_urdfParser.getModel();
	UrdfLink* const* linkPtr = model.m_links.getAtIndex(linkIndex);
	if (linkPtr)
	{

		const UrdfLink* link = *linkPtr;
	
		for (int v = 0; v < link->m_visualArray.size();v++)
		{
			const UrdfVisual& vis = link->m_visualArray[v];
			btTransform childTrans = vis.m_linkLocalFrame;
			btHashString matName(vis.m_materialName.c_str());
			UrdfMaterial *const * matPtr = model.m_materials[matName];
			if (matPtr)
			{
				UrdfMaterial *const  mat = *matPtr;
				//printf("UrdfMaterial %s, rgba = %f,%f,%f,%f\n",mat->m_name.c_str(),mat->m_rgbaColor[0],mat->m_rgbaColor[1],mat->m_rgbaColor[2],mat->m_rgbaColor[3]);
				m_data->m_linkColors.insert(linkIndex,mat->m_rgbaColor);
			}
			convertURDFToVisualShapeInternal(&vis, pathPrefix, localInertiaFrame.inverse()*childTrans, vertices, indices,textures);
		
		
		}
	}
	if (vertices.size() && indices.size())
	{
//		graphicsIndex  = m_data->m_guiHelper->registerGraphicsShape(&vertices[0].xyzw[0], vertices.size(), &indices[0], indices.size());
		//graphicsIndex  = m_data->m_guiHelper->registerGraphicsShape(&vertices[0].xyzw[0], vertices.size(), &indices[0], indices.size());
		
		//CommonRenderInterface* renderer = m_data->m_guiHelper->getRenderInterface();
		
		if (1)
		{
			int textureIndex = -1;
			if (textures.size())
			{
				
				textureIndex = m_data->m_guiHelper->registerTexture(textures[0].textureData,textures[0].m_width,textures[0].m_height);
			}
			graphicsIndex = m_data->m_guiHelper->registerGraphicsShape(&vertices[0].xyzw[0], vertices.size(), &indices[0], indices.size(),B3_GL_TRIANGLES,textureIndex);
			
		}
	}
	
	//delete textures
	for (int i=0;i<textures.size();i++)
	{
		free( textures[i].textureData);
	}
	return graphicsIndex;
}


bool BulletURDFImporter::getLinkColor(int linkIndex, btVector4& colorRGBA) const
{
	const btVector4* rgbaPtr = m_data->m_linkColors[linkIndex];
	if (rgbaPtr)
	{
		colorRGBA = *rgbaPtr;
		return true;
	}
	return false;
}

bool BulletURDFImporter::getLinkContactInfo(int linkIndex, URDFLinkContactInfo& contactInfo ) const
{
	UrdfLink* const* linkPtr = m_data->m_urdfParser.getModel().m_links.getAtIndex(linkIndex);
	if (linkPtr)
	{
		const UrdfLink* link = *linkPtr;
		contactInfo = link->m_contactInfo;
		return true;
	}
	return false;
}

void BulletURDFImporter::convertLinkVisualShapes2(int linkIndex, const char* pathPrefix, const btTransform& localInertiaFrame, class btCollisionObject* colObj, int objectIndex) const
{

  	if (m_data->m_customVisualShapesConverter)
	{
		const UrdfModel& model = m_data->m_urdfParser.getModel();
		m_data->m_customVisualShapesConverter->convertVisualShapes(linkIndex,pathPrefix,localInertiaFrame, model, colObj, objectIndex);
	}

}

int BulletURDFImporter::getNumAllocatedCollisionShapes() const
{
    return m_data->m_allocatedCollisionShapes.size();
}


btCollisionShape* BulletURDFImporter::getAllocatedCollisionShape(int index)
{
    return m_data->m_allocatedCollisionShapes[index];
}

 class btCompoundShape* BulletURDFImporter::convertLinkCollisionShapes(int linkIndex, const char* pathPrefix, const btTransform& localInertiaFrame) const
{
        
    btCompoundShape* compoundShape = new btCompoundShape();
    m_data->m_allocatedCollisionShapes.push_back(compoundShape);
    
    compoundShape->setMargin(gUrdfDefaultCollisionMargin);
	UrdfLink* const* linkPtr = m_data->m_urdfParser.getModel().m_links.getAtIndex(linkIndex);
	btAssert(linkPtr);
	if (linkPtr)
	{
		
		UrdfLink* link = *linkPtr;
		
			
		for (int v=0;v<link->m_collisionArray.size();v++)
		{
			const UrdfCollision& col = link->m_collisionArray[v];
			btCollisionShape* childShape = convertURDFToCollisionShape(&col ,pathPrefix);
			m_data->m_allocatedCollisionShapes.push_back(childShape);
			
			if (childShape)
			{
				btTransform childTrans = col.m_linkLocalFrame;
				
				compoundShape->addChildShape(localInertiaFrame.inverse()*childTrans,childShape);
			}
		}
	}
	
    return compoundShape;
}
