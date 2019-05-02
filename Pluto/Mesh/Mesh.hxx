#pragma once

#include <iostream>
#include <glm/glm.hpp>
#include <tiny_obj_loader.h>
#include <map>
#include <mutex>
#include <Eigen/Dense>
#include <Eigen/Sparse>

#include "Pluto/Tools/Options.hxx"
#include "Pluto/Libraries/Vulkan/Vulkan.hxx"
#include "Pluto/Tools/StaticFactory.hxx"

#include "./MeshStruct.hxx"

/* A mesh contains vertex information that has been loaded to the GPU. */
class Vertex;

class Mesh : public StaticFactory
{
	friend class StaticFactory;
	public:
		/* Creates a mesh component from a procedural box */
		static Mesh* CreateBox(std::string name, bool allow_edits = false, bool submit_immediately = false);
		
		/* Creates a mesh component from a procedural cone capped on the bottom */
		static Mesh* CreateCappedCone(std::string name, bool allow_edits = false, bool submit_immediately = false);
		
		/* Creates a mesh component from a procedural cylinder capped on the bottom */
		static Mesh* CreateCappedCylinder(std::string name, bool allow_edits = false, bool submit_immediately = false);
		
		/* Creates a mesh component from a procedural tube capped on both ends */
		static Mesh* CreateCappedTube(std::string name, bool allow_edits = false, bool submit_immediately = false);
		
		/* Creates a mesh component from a procedural capsule */
		static Mesh* CreateCapsule(std::string name, float radius = 1.0, float size = 0.5, int slices = 32, int segments = 4, int rings = 8, float start = 0.0, float sweep = 6.28319, bool allow_edits = false, bool submit_immediately = false);
		
		/* Creates a mesh component from a procedural cone */
		static Mesh* CreateCone(std::string name, bool allow_edits = false, bool submit_immediately = false);
		
		/* Creates a mesh component from a procedural pentagon */
		static Mesh* CreatePentagon(std::string name, bool allow_edits = false, bool submit_immediately = false);
		
		/* Creates a mesh component from a procedural cylinder (uncapped) */
		static Mesh* CreateCylinder(std::string name, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a procedural disk */
		static Mesh* CreateDisk(std::string name, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a procedural dodecahedron */
		static Mesh* CreateDodecahedron(std::string name, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a procedural plane */
		static Mesh* CreatePlane(std::string name, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a procedural icosahedron */
		static Mesh* CreateIcosahedron(std::string name, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a procedural icosphere */
		static Mesh* CreateIcosphere(std::string name, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a procedural parametric mesh. (TODO: accept a callback which given an x and y position 
			returns a Z hightfield) */
		// static Mesh* CreateParametricMesh(std::string name, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a procedural box with rounded edges */
		static Mesh* CreateRoundedBox(std::string name, float radius = .25, glm::vec3 size = glm::vec3(.75f, .75f, .75f), int slices=4, glm::ivec3 segments=glm::ivec3(1, 1, 1), bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a procedural sphere */
		static Mesh* CreateSphere(std::string name, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a procedural cone with a rounded cap */
		static Mesh* CreateSphericalCone(std::string name, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a procedural quarter-hemisphere */
		static Mesh* CreateSphericalTriangle(std::string name, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a procedural spring */
		static Mesh* CreateSpring(std::string name, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a procedural utah teapot */
		static Mesh* CreateTeapotahedron(std::string name, uint32_t segments = 8, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a procedural torus */
		static Mesh* CreateTorus(std::string name, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a procedural torus knot */
		static Mesh* CreateTorusKnot(std::string name, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a procedural triangle */
		static Mesh* CreateTriangle(std::string name, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a procedural tube (uncapped) */
		static Mesh* CreateTube(std::string name, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a procedural tube (uncapped) generated from a polyline */
		static Mesh* CreateTubeFromPolyline(std::string name, std::vector<glm::vec3> positions, float radius = 1.0, uint32_t segments = 16, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a procedural rounded rectangle tube (uncapped) generated from a polyline */
		static Mesh* CreateRoundedRectangleTubeFromPolyline(std::string name, std::vector<glm::vec3> positions, float radius = 1.0, float size_x = .75, float size_y = .75, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a procedural rectangle tube (uncapped) generated from a polyline */
		static Mesh* CreateRectangleTubeFromPolyline(std::string name, std::vector<glm::vec3> positions, float size_x = 1.0, float size_y = 1.0, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from an OBJ file (ignores .mtl files) */
		static Mesh* CreateFromOBJ(std::string name, std::string objPath, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from an ASCII STL file */
		static Mesh* CreateFromSTL(std::string name, std::string stlPath, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a GLB file (material properties are ignored) */
		static Mesh* CreateFromGLB(std::string name, std::string glbPath, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from TetGen node/element files (Can be made using "Mesh::tetrahedrahedralize") */
		static Mesh* CreateFromTetgen(std::string name, std::string path, bool allow_edits = false, bool submit_immediately = false);

		/* Creates a mesh component from a set of positions, optional normals, optional colors, optional texture coordinates, 
			and optional indices. If anything other than positions is supplied (eg normals), that list must be the same length
			as the point list. If indicies are supplied, indices must be a multiple of 3 (triangles). Otherwise, all other
			supplied per vertex data must be a multiple of 3 in length. */
		static Mesh* CreateFromRaw(
			std::string name,
			std::vector<glm::vec3> positions, 
			std::vector<glm::vec3> normals = std::vector<glm::vec3>(), 
			std::vector<glm::vec4> colors = std::vector<glm::vec4>(), 
			std::vector<glm::vec2> texcoords = std::vector<glm::vec2>(), 
			std::vector<uint32_t> indices = std::vector<uint32_t>(),
			std::vector<glm::ivec2> edges = std::vector<glm::ivec2>(),
			// when not provding rest_lengths, edges will use the distance between the positions as the default rest lengths
			// if providing rest_lengths, the size of rest_lengths must not be less than the size of edges
			// if non empty rest_legnths are provided but no edges are provided, the rest_lenghts are ignored and
			// the edges will be automatically generated using the triangles formed by the indices, default rest lengths
			// will be used
			std::vector<float> rest_lengths = std::vector<float>(),
			bool allow_edits = false, bool submit_immediately = false);

		/* Retrieves a mesh component by name */
		static Mesh* Get(std::string name);

		/* Retrieves a mesh component by id */
		static Mesh* Get(uint32_t id);

		/* Returns a pointer to the list of mesh components */
		static Mesh* GetFront();

		/* Returns the total number of reserved mesh components */
		static uint32_t GetCount();

		/* Deallocates a mesh with the given name */
		static void Delete(std::string name);

		/* Deallocates a mesh with the given id */
		static void Delete(uint32_t id);

		/* Initializes static resources */
		static void Initialize();

		/* TODO: Explain this */
		static bool IsInitialized();

		/* TODO: Explain this */
		static void CleanUp();

		/* Transfers all mesh components to an SSBO */
        static void UploadSSBO(vk::CommandBuffer command_buffer);

		/* Returns the SSBO vulkan buffer handle */
        static vk::Buffer GetSSBO();

        /* Returns the size in bytes of the current mesh SSBO */
        static uint32_t GetSSBOSize();

		/* Returns a json string summarizing the mesh */
		std::string to_string();
		
		/* If editing is enabled, returns a list of per vertex positions */
		std::vector<glm::vec3> get_positions();;

		/* If editing is enabled, returns a list of per vertex colors */
		std::vector<glm::vec4> get_colors();

		/* If editing is enabled, returns a list of per vertex normals */
		std::vector<glm::vec3> get_normals();

		/* If editing is enabled, returns a list of per vertex texture coordinates */
		std::vector<glm::vec2> get_texcoords();

		/* If editing is enabled, returns a list of edge indices */
		std::vector<uint32_t> get_edge_indices();

		/* If editing is enabled, returns a list of triangle indices */
		std::vector<uint32_t> get_triangle_indices();

		/* If editing is enabled, returns a list of tetrahedra indices */
		std::vector<uint32_t> get_tetrahedra_indices();		

		/* Returns the handle to the position buffer */
		vk::Buffer get_point_buffer();

		/* Returns the handle to the per vertex colors buffer */
		vk::Buffer get_color_buffer();

		/* Returns the handle to the triangle indices buffer */
		vk::Buffer get_triangle_index_buffer();

		/* Returns the handle to the per vertex normals buffer */
		vk::Buffer get_normal_buffer();

		/* Returns the handle to the per vertex texcoords buffer */
		vk::Buffer get_texcoord_buffer();

		/* Returns the total number of edge indices used by this mesh. 
			Divide by 2 to get the number of edges.  */
		uint32_t get_total_edge_indices();

		/* Returns the total number of triangle indices used by this mesh. 
			Divide by 3 to get the number of triangles.  */
		uint32_t get_total_triangle_indices();

		/* Returns the total number of tetrahedral indices used by this mesh. 
			Divide by 4 to get the number of tetrahedra.  */
		uint32_t get_total_tetrahedra_indices();

		/* Returns the total number of bytes per index */
		uint32_t get_index_bytes();

		/* Computes the average of all vertex positions. (centroid) 
			as well as min/max bounds and bounding sphere data. */
		void compute_metadata();

		/* Computes matrices for solving implicit Euler. Must call this before simulation. */
		void compute_simulation_matrices(float mass = 100, float stiffness = 10000, float step_size = 0.001);

		void compute_tet_simulation_matrices(float mass = 100, float stiffness = 10000, float step_size = 0.001);

		/* TODO: Explain this */
		void save_tetrahedralization(float quality_bound, float maximum_volume);

		/* Returns the last computed centroid. */
		glm::vec3 get_centroid();

		/* Returns the minimum axis aligned bounding box position */
		glm::vec3 get_min_aabb_corner();

		/* Returns the maximum axis aligned bounding box position */
		glm::vec3 get_max_aabb_corner();

		/* Returns the radius of a sphere centered at the centroid which completely contains the mesh */
		float get_bounding_sphere_radius();

		/* If mesh editing is enabled, replaces the position at the given index with a new position */
		void edit_position(uint32_t index, glm::vec3 new_position);

		/* If mesh editing is enabled, replaces the set of positions starting at the given index with a new set of positions */
		void edit_positions(uint32_t index, std::vector<glm::vec3> new_positions);

		/* If mesh editing is enabled, replaces the velocity at the given index with a new velocity */
		void edit_velocity(uint32_t index, glm::vec3 new_velocity);

		/* If mesh editing is enabled, replaces the set of velocities starting at the given index with a new set of velocities */
		void edit_velocities(uint32_t index, std::vector<glm::vec3> new_velocities);

		/* If mesh editing is enabled, replaces the normal at the given index with a new normal */
		void edit_normal(uint32_t index, glm::vec3 new_normal);

		/* If mesh editing is enabled, replaces the set of normals starting at the given index with a new set of normals */
		void edit_normals(uint32_t index, std::vector<glm::vec3> new_normals);

		/* TODO: EXPLAIN THIS */
		void compute_smooth_normals(bool upload = true);

		float get_stiffness();
		
		float get_mass();

		float get_damping_factor();

		void set_stiffness(float stiffness);

		void set_mass(float mass);

		void set_damping_factor(float damping_factor);

		/* time_step: delta_t in numerical integration, iterations: number of iterations required in 
		the projective dynamics solver, f_ext: an external field applied to every particle*/
		void update(float time_step = 0.001, uint32_t iterations = 10, glm::vec3 f_ext = glm::vec3(0,0,-9.8));

		/* If mesh editing is enabled, replaces the vertex color at the given index with a new vertex color */
		void edit_vertex_color(uint32_t index, glm::vec4 new_color);

		/* If mesh editing is enabled, replaces the set of vertex colors starting at the given index with a new set of vertex colors */
		void edit_vertex_colors(uint32_t index, std::vector<glm::vec4> new_colors);

		/* If mesh editing is enabled, replaces the texture coordinate at the given index with a new texture coordinate */
		void edit_texture_coordinate(uint32_t index, glm::vec2 new_texcoord);

		/* If mesh editing is enabled, replaces the set of texture coordinates starting at the given index with a new set of texture coordinates */
		void edit_texture_coordinates(uint32_t index, std::vector<glm::vec2> new_texcoords);
		
		/* If RTX Raytracing is enabled, builds a low level BVH for this mesh. */
		void build_low_level_bvh(bool submit_immediately = false);

		/* If RTX Raytracing is enabled, builds a top level BVH for all created meshes. (TODO, account for mesh transformations) */
		static void build_top_level_bvh(bool submit_immediately = false);

		/* TODO */
		void show_bounding_box(bool should_show);

		/* TODO */
		bool should_show_bounding_box();

	private:
		/* Creates an uninitialized mesh. Useful for preallocation. */
		Mesh();

		/* Creates a mesh with the given name and id. */
		Mesh(std::string name, uint32_t id);

		/* TODO */
		static std::mutex creation_mutex;
		
		/* TODO */
		static bool Initialized;
		
		/* A list of the mesh components, allocated statically */
		static Mesh meshes[MAX_MESHES];

		/* A lookup table of name to mesh id */
		static std::map<std::string, uint32_t> lookupTable;

		/* A pointer to the mapped mesh SSBO. This memory is shared between the GPU and CPU. */
        static MeshStruct* pinnedMemory;

		/* A vulkan buffer handle corresponding to the mesh SSBO  */
        static vk::Buffer SSBO;

        /* The corresponding mesh SSBO memory */
        static vk::DeviceMemory SSBOMemory;

		/* TODO  */
        static vk::Buffer stagingSSBO;

        /* TODO */
        static vk::DeviceMemory stagingSSBOMemory;

		/* The structure containing all shader mesh properties. This is what's coppied into the SSBO per instance */
        MeshStruct mesh_struct;

		/* A handle to an RTX top level BVH */
		static vk::AccelerationStructureNV topAS;
		static vk::DeviceMemory topASMemory;

		/* A handle to an RTX buffer of geometry instances used to build the top level BVH */
		static vk::Buffer instanceBuffer;
		static vk::DeviceMemory instanceBufferMemory;

		/* Lists of per vertex data. These might not match GPU memory if editing is disabled. */
		std::vector<glm::vec3> positions;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec4> colors;
		std::vector<glm::vec2> texcoords;
		std::vector<uint32_t> tetrahedra_indices;
		std::vector<uint32_t> triangle_indices;
		std::vector<uint32_t> edge_indices;

		/* Additions from Daqi */
		std::vector<glm::vec3> velocities;

		bool isTet = false;

		// For mass spring system
		std::vector<std::pair<uint32_t, uint32_t>> edges; // pairs of indices
		std::vector<float> rest_lengths; // difference of initial positions of the two vertices in each edge
		
		// For tetrahedra mesh
		struct Tet
		{
			int v[4];
			Tet(int v0, int v1, int v2, int v3) { v[0] = v0; v[1] = v1; v[2] = v2; v[3] = v3; }
		};

		std::vector<Tet> tets; 
		std::vector<Eigen::Matrix3f> Dms;
		std::vector<Eigen::Matrix3f> invDms;

		//
		Eigen::MatrixXf G; //gravity, hardcoded to (0,0,-9.8)
		Eigen::SparseMatrix<float> L, J, M;
		float mass; //using a hardcoded mass of 100 now
		float stiffness; // using a hardcoded stiffness of 10000 now;
		float step_size; // initialized to 0.001
		float damping_factor; //initialized to 0;
		std::shared_ptr<Eigen::SimplicialLLT<Eigen::SparseMatrix<float>>> precomputed_cholesky;

		/* A handle to the attributes loaded from tiny obj */
		tinyobj::attrib_t attrib;

		/* A handle to the buffer containing per vertex positions */
		vk::Buffer pointBuffer;
		vk::DeviceMemory pointBufferMemory;

		/* A handle to the buffer containing per vertex colors */
		vk::Buffer colorBuffer;
		vk::DeviceMemory colorBufferMemory;

		/* A handle to the buffer containing per vertex normals */
		vk::Buffer normalBuffer;
		vk::DeviceMemory normalBufferMemory;

		/* A handle to the buffer containing per vertex texture coordinates */
		vk::Buffer texCoordBuffer;
		vk::DeviceMemory texCoordBufferMemory;

		/* A handle to the buffer containing triangle indices */		
		vk::Buffer triangleIndexBuffer;
		vk::DeviceMemory triangleIndexBufferMemory;

		/* Declaration of an RTX geometry instance. This struct is described in the Khronos 
			specification to be exactly this, so don't modify! */
		struct VkGeometryInstance
		{
			float transform[12];
			uint32_t instanceId : 24;
			uint32_t mask : 8;
			uint32_t instanceOffset : 24;
			uint32_t flags : 8;
			uint64_t accelerationStructureHandle;
		};

		/* An RTX geometry handle */
		vk::GeometryNV geometry;

		/* An RTX instance handle */
		VkGeometryInstance instance;

		/* An RTX handle to the low level acceleration structure */
		vk::AccelerationStructureNV lowAS;
		vk::DeviceMemory lowASMemory;

		/* True if the low level BVH was built. (TODO: make false if mesh edits were submitted) */
		bool lowBVHBuilt = false;
		
		/* True if this mesh component supports editing. If false, indices are automatically generated. */
		bool allowEdits = false;

		/* TODO */
		bool showBoundingBox = false;

		/* Frees any vulkan resources this mesh component may have allocated */
		void cleanup();

		/* Creates a generic vertex buffer object */
		void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer &buffer, vk::DeviceMemory &bufferMemory);

		/* Creates a position buffer, and uploads position data stored in the positions list */
		void createPointBuffer(bool allow_edits, bool submit_immediately);

		/* Creates a per vertex color buffer, and uploads per vertex color data stored in the colors list */
		void createColorBuffer(bool allow_edits, bool submit_immediately);

		/* Creates a normal buffer, and uploads normal data stored in the normals list */
		void createNormalBuffer(bool allow_edits, bool submit_immediately);

		/* Creates a texture coordinate buffer, and uploads texture coordinate data stored in the texture coordinates list */
		void createTexCoordBuffer(bool allow_edits, bool submit_immediately);

		/* Creates an index buffer, and uploads index data stored in the indices list */
		void createTriangleIndexBuffer(bool allow_edits, bool submit_immediately);

		/* Loads in an OBJ mesh and copies per vertex data to the GPU */
		void load_obj(std::string objPath, bool allow_edits, bool submit_immediately);

		/* Loads in an STL mesh and copies per vertex data to the GPU */
		void load_stl(std::string stlPath, bool allow_edits, bool submit_immediately);

		/* Loads in a GLB mesh and copies per vertex data to the GPU */
		void load_glb(std::string glbPath, bool allow_edits, bool submit_immediately);

		/* TODO: Explain this */
		void load_tetgen(std::string path, bool allow_edits, bool submit_immediately);

		/* Copies per vertex data to the GPU */
		void load_raw (
			std::vector<glm::vec3> &positions, 
			std::vector<glm::vec3> &normals, 
			std::vector<glm::vec4> &colors, 
			std::vector<glm::vec2> &texcoords,
			std::vector<uint32_t> indices,
			std::vector<glm::ivec2>& edges,
			std::vector<float>& rest_lengths,
			bool allow_edits,
			bool submit_immediately
		);

		void generate_edges();
		/* need to call this whenever the mass changes*/
		void precompute_mass_matrix();
		/* need to call this whenever the step size or stiffness, or mass changes*/
		void precompute_cholesky();
		
		/* Creates a procedural mesh from the given mesh generator, and copies per vertex to the GPU */
		template <class Generator>
		void make_primitive(Generator &mesh, bool allow_edits, bool submit_immediately)
		{
			std::vector<Vertex> vertices;

			auto genVerts = mesh.vertices();
			while (!genVerts.done()) {
				auto vertex = genVerts.generate();
				positions.push_back(vertex.position);
				normals.push_back(vertex.normal);
				texcoords.push_back(vertex.texCoord);
				colors.push_back(glm::vec4(0.0, 0.0, 0.0, 0.0));
				genVerts.next();
			}

			auto genTriangles = mesh.triangles();
			while (!genTriangles.done()) {
				auto triangle = genTriangles.generate();
				triangle_indices.push_back(triangle.vertices[0]);
				triangle_indices.push_back(triangle.vertices[1]);
				triangle_indices.push_back(triangle.vertices[2]);
				genTriangles.next();
			}

			allowEdits = allow_edits;

			cleanup();
			compute_metadata();
			createPointBuffer(allow_edits, submit_immediately);
			createColorBuffer(allow_edits, submit_immediately);
			createNormalBuffer(allow_edits, submit_immediately);
			createTexCoordBuffer(allow_edits, submit_immediately);
			createTriangleIndexBuffer(allow_edits, submit_immediately);
		}
};
