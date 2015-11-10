#include "anvil.hpp"

using namespace math;
using namespace util;
using namespace gfx;

struct Object
{
    Pose pose;
    float3 scale;
    Object() : scale(1, 1, 1) {}
    float4x4 get_model() const { return mul(pose.matrix(), make_scaling_matrix(scale)); }
    math::Box<float, 3> bounds;
    
    bool check_hit(const Ray & worldRay) const
    {
        auto localRay = pose.inverse() * worldRay;
        localRay.origin /= scale;
        localRay.direction /= scale;
        return intersect_ray_box(localRay, bounds.min, bounds.max);
    }
};

struct ModelObject : public Object
{
    GlMesh mesh;
    void draw() const { mesh.draw_elements(); };
    void build(const Geometry & g)
    {
        mesh = make_mesh_from_geometry(g);
        bounds = g.compute_bounds();
    };
};

struct LightObject : public Object
{
    float3 color;
};

struct TexturedObject : public ModelObject
{
    GlTexture diffuseTexture;
    GlTexture normalTexture;
};

class Raycaster
{
    Bounds rect;
    float4x4 invViewProj;
public:
    
    Raycaster(const Bounds rect, const float4x4 & proj, const Pose & view)
    {
        this->rect = rect;
        invViewProj = inv(mul(proj, look_at_matrix_rh(view.position, view.position + view.ydir(), view.zdir())));
    }
    
    Ray compute(const float2 & pixel) const
    {
        auto viewX = (pixel.x - rect.x0) * 2.0f / rect.width() - 1;
        auto viewY = 1 - (pixel.y - rect.y0) * 2.0f / rect.height();
        return between(transform_coord(invViewProj, {viewX, viewY, -1}), transform_coord(invViewProj, {viewX, viewY, 1}));
    }
};

class LinearTranslationDragger
{
    ModelObject & object;
    Raycaster caster;
    float3 direction, initialPosition;
    float initialS;
    
    float ComputeS(const float2 & mouse) const
    {
        const Ray ray1 = {initialPosition, direction}, ray2 = caster.compute(mouse);
        const auto r12 = ray2.origin - ray1.origin;
        const auto e1e2 = dot(ray1.direction, ray2.direction), denom = 1 - e1e2 * e1e2;
        return (dot(r12,ray1.direction) - dot(r12,ray2.direction) * e1e2) / denom;
    }
    
public:
    
    LinearTranslationDragger(ModelObject & object, const Raycaster & caster, const float3 & direction, const float2 & click) : object(object), caster(caster), direction(qrot(object.pose.orientation, direction)), initialPosition(object.pose.position), initialS(ComputeS(click)) {}
    
    void OnDrag(float2 newMouse) { object.pose.position = initialPosition + direction * (ComputeS(newMouse) - initialS); }
    void OnRelease() {}
    void OnCancel() { object.pose.position = initialPosition; }
};


struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;

    GlCamera camera;
    PreethamProceduralSky preethamSky;
    RenderableGrid grid;
    FPSCameraController cameraController;
    
    std::unique_ptr<GlShader> simpleShader;
    
    std::vector<ModelObject> models;
    std::vector<LightObject> lights;
    std::vector<TexturedObject> texturedModels;
    
    std::vector<ModelObject> proceduralModels;
    std::vector<ModelObject> debugModels;
    
    ModelObject boxSelection;
    
    int selectedObjectIndex = -1;
    
    bool isDragging = false;
    
    Ray initialRay;
    float3 initialPosition;
    
    std::shared_ptr<LinearTranslationDragger> dragger;
    
    ExperimentalApp() : GLFWApp(640, 480, "Geometry App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        grid = RenderableGrid(1, 100, 100);
        cameraController.set_camera(&camera);
        
        boxSelection.build(make_cube());
        boxSelection.scale = {4, .25, .25};
        //boxSelection.mesh.set_non_indexed(GL_LINES);
        
        lights.resize(2);
        
        lights[0].color = float3(44.f / 255.f, 168.f / 255.f, 220.f / 255.f);
        lights[0].pose.position = float3(25, 15, 0);
        
        lights[1].color = float3(220.f / 255.f, 44.f / 255.f, 201.f / 255.f);
        lights[1].pose.position = float3(-25, 15, 0);
        
        simpleShader.reset(new gfx::GlShader(read_file_text("assets/shaders/simple_vert.glsl"), read_file_text("assets/shaders/simple_frag.glsl")));
        
        {
            proceduralModels.resize(11);
            
            proceduralModels[0].build(make_sphere(1.0));
            proceduralModels[0].pose.position = float3(3, 0, 2);
            
            proceduralModels[1].build(make_cube());
            proceduralModels[1].pose.position = float3(7, 0, 5);
            
            proceduralModels[2].build(make_frustum());
            proceduralModels[2].pose.position = float3(0, 0, 6);
            
            proceduralModels[3].build(make_torus());
            proceduralModels[3].pose.position = float3(10, 4, -10);
            
            proceduralModels[4].build(make_capsule(8, 1, 3));
            proceduralModels[4].pose.position = float3(5, 0, 10);
            
            proceduralModels[5].build(make_plane(2, 2, 5, 5));
            proceduralModels[5].pose.position = float3(-5, 0, 2);
            
            proceduralModels[6].build(make_axis());
            proceduralModels[6].pose.position = float3(-5, 2, 4);
            
            proceduralModels[7].build(make_spiral());
            proceduralModels[7].pose.position = float3(-5, 0, 6);
            
            proceduralModels[8].build(make_icosahedron());
            proceduralModels[8].pose.position = float3(-10, 0, 8);
            
            proceduralModels[9].build(make_octohedron());
            proceduralModels[9].pose.position = float3(-15, 0, 10);
            
            proceduralModels[10].build(make_tetrahedron());
            proceduralModels[10].pose.position = float3(-20, 0, 12);
        }
        
        {
            debugModels.resize(3);
            debugModels[0].mesh = make_mesh_from_geometry(load_geometry_from_ply("assets/models/geometry/CubeHollow2Sides.ply"));
            debugModels[0].pose.position = float3(4, -2, 4);
            
            debugModels[1].mesh = make_mesh_from_geometry(load_geometry_from_ply("assets/models/geometry/CylinderUniform.ply"));
            debugModels[1].pose.position = float3(-4, -2, -4);
            
            debugModels[2].mesh = make_mesh_from_geometry(load_geometry_from_ply("assets/models/shaderball/shaderball.ply"));
            debugModels[2].pose.position = float3(0, -4, 0);
        }
        
        gfx::gl_check_error(__FILE__, __LINE__);
    }
    
    void on_window_resize(math::int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        if (event.type == InputEvent::CURSOR && isDragging)
        {
            if (selectedObjectIndex >= 0)
            {
                if (dragger)
                    dragger->OnDrag(event.cursor);
                
                //std::cout << selectedObjectIndex << " - " << selectedObject.pose.position.x << std::endl;
                
                
            }
        }
        
        if (event.type == InputEvent::MOUSE && event.action == GLFW_PRESS)
        {
            if (event.value[0] == GLFW_MOUSE_BUTTON_LEFT)
            {
                int width, height;
                glfwGetWindowSize(window, &width, &height);
                
                int objIdx = -1;
                for (const auto & model : proceduralModels)
                {
                    auto worldRay = camera.get_world_ray(event.cursor, float2(width, height));
                    if (model.check_hit(worldRay))
                    {
                        boxSelection.pose.position = model.pose.position;
                        selectedObjectIndex = objIdx + 1;
                        std::cout << "New selected object: " << selectedObjectIndex << std::endl;
                        break;
                    }
                    objIdx++;
                }
                
                
                if (selectedObjectIndex >= 0)
                {
                    Raycaster caster({0, 0, (float) event.windowSize.x, (float) event.windowSize.y}, make_perspective_matrix_rh_gl(1, (event.windowSize.x / event.windowSize.y), 0.25f, 32.f), camera.pose);
                    // Ray ray = caster.compute(event.cursor);
                    
                    auto & selectedObject = proceduralModels[selectedObjectIndex];
                    initialPosition = selectedObject.pose.position;
                    
                    float3 axis = float3(0, 0, 1);
                    
                    dragger = std::make_shared<LinearTranslationDragger>(selectedObject, caster, axis, event.cursor);
                }

            }
        }
        
        
        if (event.type == InputEvent::MOUSE)
        {
            if (event.is_mouse_down())
            {
                isDragging = true;
                
                auto worldRay = camera.get_world_ray(event.cursor, float2(event.windowSize.x, event.windowSize.y));
                if (boxSelection.check_hit(worldRay))
                {
                    initialRay = worldRay;
                    std::cout << "set initial ray... " << std::endl;

                }
                

              
            }
            
            if (event.is_mouse_up())
            {
                isDragging = false;
            }
        }
        
        cameraController.handle_input(event);
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
    }
    
    void on_draw() override
    {
        glfwMakeContextCurrent(window);
        
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
     
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.1f, 0.1f, 0.5f, 1.0f);

        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
        
        preethamSky.render(viewProj, camera.get_eye_point(), camera.farClip);
        
        // Simple Shader
        {
            simpleShader->bind();
            
            simpleShader->uniform("u_viewProj", viewProj);
            simpleShader->uniform("u_eye", camera.get_eye_point());
            
            simpleShader->uniform("u_emissive", float3(.10f, 0.10f, 0.10f));
            simpleShader->uniform("u_diffuse", float3(0.4f, 0.4f, 0.25f));
            
            for (int i = 0; i < lights.size(); i++)
            {
                auto light = lights[i];
                
                simpleShader->uniform("u_lights[" + std::to_string(i) + "].position", light.pose.position);
                simpleShader->uniform("u_lights[" + std::to_string(i) + "].color", light.color);
            }

            for (const auto & model : proceduralModels)
            {
                simpleShader->uniform("u_modelMatrix", model.get_model());
                simpleShader->uniform("u_modelMatrixIT", inv(transpose(model.get_model())));
                model.draw();
            }
            
            for (const auto & model : debugModels)
            {
                auto modelMat = model.get_model() * make_scaling_matrix(0.0125f);
                simpleShader->uniform("u_modelMatrix", modelMat);
                simpleShader->uniform("u_modelMatrixIT", inv(transpose(modelMat)));
                model.draw();
            }
            
            {
                auto modelMat = boxSelection.get_model();
                simpleShader->uniform("u_modelMatrix", modelMat);
                simpleShader->uniform("u_modelMatrixIT", inv(transpose(modelMat)));
                boxSelection.draw();
            }
            
            simpleShader->unbind();
        }
        
        grid.render(proj, view, {0, -5, 0});

        gfx::gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
