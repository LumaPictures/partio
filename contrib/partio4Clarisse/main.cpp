#include <of_object.h>
#include <of_attr.h>
#include <of_app.h>
#include <dso_export.h>

#include <gmath.h>
#include <module.h>
#include <resource_property.h>
#include <geometry_point_cloud.h>
#include <poly_mesh.h>
#include <poly_mesh_property.h>
#include <app_progress_bar.h>
#include <core_vector.h>
#include <resource_property.h>
#include <geometry_point_property.h>
#include <geometry_property_collection.h>
#include <particle_cloud.h>

#include <Partio.h>
#include <PartioAttribute.h>
#include <PartioIterator.h>

#include "partio.cma"

namespace{    
    struct PartioData{
        CoreString cache_path;
        Partio::ParticlesData* particles;
        
        PartioData() : cache_path(""), particles(0)
        { }
        
        ~PartioData()
        {
            release();
        }
        
        Partio::ParticlesData* load_object(const CoreString& path)
        {
            if (cache_path == path)
                return particles;
            else
            {
                release();           
                cache_path = path;
                particles = Partio::read(cache_path.get_data());
                return particles;
            }
        }
        
        void release()
        {
            if (particles)
            { // destructor is private
                particles->release();
                particles = 0;
            }            
            cache_path = "";
        }
    };    
}

IX_BEGIN_DECLARE_MODULE_CALLBACKS(GeometryCsdkPartio, ModuleGeometryCallbacks)
    static void init_class(OfClass& cls);
    static ResourceData* create_resource(OfObject& object, const int& resource_id, void* data);
    static void* create_module_data(const OfObject& object);
    static bool destroy_module_data(const OfObject& object, void* data);
IX_END_DECLARE_MODULE_CALLBACKS(GeometryCsdkPartio)

IX_BEGIN_EXTERN_C
    DSO_EXPORT void
    on_register_module(OfApp& app, CoreVector<OfClass *>& new_classes)
    {
        OfClass *new_class = IX_DECLARE_MODULE_CLASS(GeometryCsdkPartio);
        new_classes.add(new_class);

        IX_MODULE_CLBK *module_callbacks;
        IX_CREATE_MODULE_CLBK(new_class, module_callbacks)
        IX_MODULE_CLBK::init_class(*new_class);
        module_callbacks->cb_create_resource = IX_MODULE_CLBK::create_resource;
        module_callbacks->cb_create_module_data = IX_MODULE_CLBK::create_module_data;
        module_callbacks->cb_destroy_module_data = IX_MODULE_CLBK::destroy_module_data;
    }
IX_END_EXTERN_C

void
IX_MODULE_CLBK::init_class(OfClass& cls)
{
    // Set which attributes the geometry resource depends on.
    CoreVector<CoreString> attrs;
    attrs.add("cache_path");
    cls.set_resource_attrs(ModuleGeometry::RESOURCE_ID_GEOMETRY, attrs);
    
    // If we want to add some properties to the mesh, we also need to set
    // the dependencies for the properties resource (this is optional).
    // Here we are setting a dependency on the geometry resource because we'll
    // need it.
    CoreVector<int> deps;
    deps.add(ModuleGeometry::RESOURCE_ID_GEOMETRY);
    cls.set_resource_deps(ModuleGeometry::RESOURCE_ID_GEOMETRY_PROPERTIES, deps);
    // We are adding the exported_properties as a dependency
    // to geometry properties
    CoreVector<CoreString> property_attrs;
    property_attrs.add("exported_properties");
    cls.set_resource_attrs(ModuleGeometry::RESOURCE_ID_GEOMETRY_PROPERTIES, property_attrs);
}

ResourceData *
IX_MODULE_CLBK::create_resource(OfObject& object, const int& resource_id, void *data)
{
    if (resource_id == ModuleGeometry::RESOURCE_ID_GEOMETRY)
    { // create the geometry resource        
        PartioData* local_data = reinterpret_cast<PartioData*>(object.get_module_data());
        
    	const CoreString cache_path = object.get_attribute("cache_path")->get_string();
    	if (cache_path.get_length() == 0) // TODO : add proper checks later on
    		return 0;

    	Partio::ParticlesData* particles = local_data->load_object(cache_path);
        
        if (particles == 0)
            return 0;
        
        const int numParticles = particles->numParticles();
        
        CoreString comment;        
        comment << "Number of particles : " << numParticles << "\n";
        
        const int numAttributes = particles->numAttributes();
        
        Partio::ParticleAttribute positionAttr;
        Partio::ParticleAttribute velocityAttr;
        positionAttr.type = Partio::NONE;
        velocityAttr.type = Partio::NONE;
        
        // Write the list of available properties to
        // the comment        
        comment << numAttributes << " Available Properties : \n";
        
        for (int i = 0; i < numAttributes; ++i)
        {
            Partio::ParticleAttribute attr;
            particles->attributeInfo(i, attr);
            comment << " - " << attr.name.c_str() << " [" << attr.count << " of ";
            switch(attr.type)
            {
                case Partio::NONE:
                    comment << "None";
                    break;
                case Partio::VECTOR:
                    comment << "Vector";
                    break;
                case Partio::FLOAT:
                    comment << "Float";
                    break;
                case Partio::INT:
                    comment << "Integer";
                    break;
                case Partio::INDEXEDSTR:
                    comment << "Indexed String";
                    break;
                default:
                    comment << "Unknown";
            }
            comment << "]\n";
            
            if (((attr.name == "position") || (attr.name == "Position") || (attr.name == "p"))
                    && ((attr.type == Partio::VECTOR) && (attr.count == 3)))
                positionAttr = attr;
            if (((attr.name == "velocity") || (attr.name == "Velocity") || (attr.name == "v"))
                    && ((attr.type == Partio::VECTOR) && (attr.count == 3)))
                velocityAttr = attr;
        }

        if (positionAttr.type == Partio::NONE)
            return 0;
        
        comment << "Geometry Properties :\n - " << positionAttr.name.c_str() << "\n";
   	
    	const bool hasVelocity = velocityAttr.type != Partio::NONE;
        
        CoreArray<GMathVec3f> vertices(numParticles);
    	CoreArray<GMathVec3f> velocities;
        CoreArray<GMathVec3f> normals;

    	for (int i = 0; i < numParticles; ++i)
    	{
    		const float* d = particles->data<float>(positionAttr, i);
    		vertices[i][0] = d[0];
    		vertices[i][1] = d[1];
    		vertices[i][2] = d[2];
    	}   	

    	if (hasVelocity)
    	{
            comment << " - " << velocityAttr.name.c_str() << "\n";
            velocities.resize(numParticles);
    		for (int i = 0; i < numParticles; ++i)
			{
				const float* d = particles->data<float>(velocityAttr, i);
				velocities[i][0] = d[0];
				velocities[i][1] = d[1];
				velocities[i][2] = d[2];
			}
    	}
        
        // Hardcoding normals to point upwards
        normals.resize(numParticles);
        for (int i = 0; i < numParticles; ++i)
        {
            normals[i][0] = 0.0f;
            normals[i][1] = 1.0f;
            normals[i][2] = 0.0f;
        }
        
        GeometryPointCloud* pointcloud_geometry = new GeometryPointCloud();
        pointcloud_geometry->init(vertices.get_count(), &vertices[0], hasVelocity ? &velocities[0] : 0, &normals[0]);       
              
        ParticleCloud* particles_geometry = new ParticleCloud(*pointcloud_geometry);
        
        object.set_comment(comment);
        
        // if there are no exported attributes
        // we can free the data, because
        // we don't expect anyone to request the data
        // and even if they request it, we won't export anything        
        if (object.get_attribute("exported_properties")->get_string().get_length() == 0)
            local_data->release();
        
        return particles_geometry;
    }
    else if (resource_id == ModuleGeometry::RESOURCE_ID_GEOMETRY_PROPERTIES)
    { // create the properties (optional)
        const CoreString exported_properties = object.get_attribute("exported_properties")->get_string();
        if (exported_properties.get_length() == 0)
            return 0;
        PartioData* local_data = reinterpret_cast<PartioData*>(object.get_module_data());
        
        ModuleGeometry* module = CoreBaseObject::cast<ModuleGeometry>(object.get_module());        
        if (module == 0)
            return 0;
        
        const ParticleCloud* geometry_particles = CoreBaseObject::cast<ParticleCloud>(module->get_geometry()); // geometry resource
        if (geometry_particles == 0)
            return 0;

        const GeometryPointCloud* geometry_pointcloud = geometry_particles->get_point_cloud();
        
        if (geometry_pointcloud == 0)
            return 0;
        
        const int num_particles = geometry_pointcloud->get_point_count();
        // path has to be recalculated in case
        // we already loaded the cache and changed
        // the exported attributes which doesn't trigger a
        // geometry re-export
        const CoreString cache_path = object.get_attribute("cache_path")->get_string();
    	if (cache_path.get_length() == 0) // TODO : add proper checks later on
    		return 0;
        
        Partio::ParticlesData* particles = local_data->load_object(cache_path);
        if (particles == 0) // if file can't be loaded, no need to release this
            return 0;
        
        if (num_particles != particles->numParticles())
        {
            local_data->release();
            return 0;
        }
        
        CoreVector<GeometryProperty*> properties;
        CoreVector<CoreString> properties_to_export = exported_properties.split(" ");
        const int num_properties_to_export = properties_to_export.get_count();
        
        for (int i = 0; i < num_properties_to_export; ++i)
        {
            const CoreString& attr_name = properties_to_export[i];
            Partio::ParticleAttribute attr;
            if (particles->attributeInfo(attr_name.get_data(), attr))
            {
                if ((attr.type == Partio::NONE) || (attr.type == Partio::INDEXEDSTR))
                    continue;
                ResourceProperty* property = new ResourceProperty(attr_name);
                ResourceProperty::Type property_type = ResourceProperty::TYPE_INT_8;
                if (attr.type == Partio::INT)
                {
                    property_type = ResourceProperty::TYPE_INT_32;
                    property->init(property_type, attr.count, num_particles);
                    CoreVector<int> property_data(num_particles * attr.count);
                    for (int j = 0; j < num_particles; ++j)
                    {
                        const int* d = particles->data<int>(attr, j);
                        const int id = j * attr.count;
                        for (int k = 0; k < attr.count; ++k)
                            property_data[id + k] = d[k];
                    }
                    property->set_values(property_data.get_data());
                }
                else // Vector and Float
                {
                    property_type = ResourceProperty::TYPE_FLOAT_32;
                    property->init(property_type, attr.count, num_particles);
                    CoreVector<float> property_data(num_particles * attr.count);
                    for (int j = 0; j < num_particles; ++j)
                    {
                        const float* d = particles->data<float>(attr, j);
                        const int id = j * attr.count;
                        for (int k = 0; k < attr.count; ++k)
                            property_data[id + k] = d[k];
                    }
                    property->set_values(property_data.get_data());                    
                }
                GeometryPointProperty* point_property = new GeometryPointProperty(attr_name, GMathTimeSampling(0.0), property_type);
                point_property->init(0, property);
                properties.add(point_property);               
            }
        }

        // The returned resource must be a GeometryPropertyArray:
        GeometryPropertyCollection* property_array = new GeometryPropertyCollection;
        //GeometryPropertyArray* property_array = new GeometryPropertyArray();
        property_array->set(properties);
        // release the data, we won't need this anymore
        local_data->release();
        return property_array;
    } else {
        return 0;
    }
}

void*
IX_MODULE_CLBK::create_module_data(const OfObject& object)
{
    return new PartioData();
}

bool
IX_MODULE_CLBK::destroy_module_data(const OfObject& object, void* data)
{
    delete reinterpret_cast<PartioData*>(data);
    return true;
}
