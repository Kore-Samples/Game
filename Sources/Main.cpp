#include "pch.h"

#include <Kore/IO/FileReader.h>
#include <Kore/Math/Core.h>
#include <Kore/System.h>
#include <Kore/Input/Keyboard.h>
#include <Kore/Input/Mouse.h>
#include <Kore/Graphics1/Image.h>
#include <Kore/Graphics4/Graphics.h>
#include <Kore/Graphics4/PipelineState.h>
#include <Kore/Graphics1/Color.h>
#include <Kore/Log.h>
#include "MeshObject.h"

#ifdef KORE_VR
#include <Kore/Math/Quaternion.h>
#include <Kore/Vr/VrInterface.h>
#include <Kore/Vr/SensorState.h>
#endif

using namespace Kore;

#define CAMERA_ROTATION_SPEED 0.001

namespace {
    const int width = 1024;
    const int height = 768;
    double startTime;
    Graphics4::Shader* vertexShader;
	Graphics4::Shader* fragmentShader;
	Graphics4::PipelineState* pipeline;
    
    // null terminated array of MeshObject pointers
    MeshObject* objects[] = { nullptr, nullptr, nullptr, nullptr, nullptr };
    int renderObjectNum = 0;

	MeshObject* tiger;

	bool sit = false;
    
    // uniform locations - add more as you see fit
	Graphics4::TextureUnit tex;
	Graphics4::ConstantLocation pLocation;
	Graphics4::ConstantLocation vLocation;
	Graphics4::ConstantLocation mLocation;

    vec3 playerPosition;
    vec3 globe;
    
    bool left, right, up, down, forward, backward;
	
	bool rotate = false;
	int mousePressX, mousePressY;

	bool debug = false;
    
	void initCamera() {
#ifdef KORE_VR
		playerPosition = vec3(0, 0, 0);
#else
		playerPosition = vec3(0, 0, 20); 
#endif
		globe = vec3(0, Kore::pi, 0);
	}

#ifdef KORE_VR
	mat4 getViewMatrix(SensorState& state) {

		Quaternion orientation = state.pose.vrPose.orientation;
		vec3 position = state.pose.vrPose.position;

		if (debug) {
			log(Info, "Pos %f %f %f", position.x(), position.y(), position.z());
			log(Info, "Player Pos %f %f %f", playerPosition.x(), playerPosition.y(), playerPosition.z());
		}

		// Get view matrices
		mat4 rollPitchYaw = orientation.matrix();
		vec3 up = vec3(0, 1, 0);
		vec3 eyePos = rollPitchYaw * vec4(position.x(), position.y(), position.z(), 0);
		eyePos += playerPosition;
		vec3 lookAt = eyePos + vec3(0, 0, -1);
		mat4 view = mat4::lookAt(eyePos, lookAt, up);

		return view;
	}

	mat4 getOrthogonalProjection(float left, float right, float up, float down, float zn, float zf) {
		float projXScale = 2.0f / (left + right);
		float projXOffset = (left - right) * projXScale * 0.5f;
		float projYScale = 2.0f / (up + down);
		float projYOffset = (up - down) * projYScale * 0.5f;

		bool flipZ = false;

		mat4 m = mat4::Identity();
		m.Set(0, 0, 2 / (left + right));
		m.Set(0, 1, 0);
		m.Set(0, 2, projXOffset);
		m.Set(0, 3, 0);
		m.Set(1, 0, 0);
		m.Set(1, 1, projYScale);
		m.Set(1, 2, -projYOffset);
		m.Set(1, 3, 0);
		m.Set(2, 0, 0);
		m.Set(2, 1, 0);
		m.Set(2, 2, -1.0f * (zn + zf) / (zn - zf));
		m.Set(2, 3, 2.0f * (zf * zn) / (zn - zf));
		m.Set(3, 0, 0.0f);
		m.Set(3, 1, 0.0f);
		m.Set(3, 2, 1.0f);
		m.Set(3, 3, 0.0f);
		return m;
	}

	mat4 getProjectionMatrix(SensorState& state) {
		float left = state.pose.vrPose.left;
		float right = state.pose.vrPose.right;
		float bottom = state.pose.vrPose.bottom;
		float top = state.pose.vrPose.top;

		// Get projection matrices
		mat4 proj = mat4::Perspective(45, (float)width / (float)height, 0.1f, 100.0f);
		//mat4 proj = mat4::orthogonalProjection(left, right, top, bottom, 0.1f, 100.0f); // top and bottom are same
		//mat4 proj = getOrthogonalProjection(left, right, top, bottom, 0.1f, 100.0f);
		return proj;
	}

#endif

    void update() {
        float t = (float)(System::time() - startTime);

		const float speed = 0.05f;
		if (left) {
			playerPosition.x() -= speed;
		}
		if (right) {
			playerPosition.x() += speed;
		}
		if (forward) {
			playerPosition.z() += speed;
		}
		if (backward) {
			playerPosition.z() -= speed;
		}
		if (up) {
			playerPosition.y() += speed;
		}
		if (down) {
			playerPosition.y() -= speed;
		}

		Graphics4::begin();
		Graphics4::clear(Graphics4::ClearColorFlag | Graphics4::ClearDepthFlag, Graphics1::Color::Black, 1.0f, 0);

		
		
#ifdef KORE_VR

		VrInterface::begin();
		SensorState state;
		for (int eye = 0; eye < 2; ++eye) {
			VrInterface::beginRender(eye);

			Graphics4::clear(Graphics4::ClearColorFlag | Graphics4::ClearDepthFlag, Graphics1::Color::Black, 1.0f, 0);

			Graphics4::setPipeline(pipeline);

			state = VrInterface::getSensorState(eye);
			mat4 view = getViewMatrix(state);
			mat4 proj = getProjectionMatrix(state);

			Graphics4::setMatrix(vLocation, view);
			Graphics4::setMatrix(pLocation, proj);

#ifdef KORE_STEAMVR
			Graphics4::setMatrix(vLocation, state.pose.vrPose.eye);
			Graphics4::setMatrix(pLocation, state.pose.vrPose.projection);
#endif

			// Render world
			Graphics4::setMatrix(mLocation, tiger->M);
			tiger->render(tex);

			VrInterface::endRender(eye);
		}

		VrInterface::warpSwap();

		Graphics4::restoreRenderTarget();
		Graphics4::clear(Graphics4::ClearColorFlag | Graphics4::ClearDepthFlag, Graphics1::Color::Black, 1.0f, 0);

		mat4 view = getViewMatrix(state);
		mat4 proj = getProjectionMatrix(state);

		Graphics4::setMatrix(vLocation, view);
		Graphics4::setMatrix(pLocation, proj);

#ifdef KORE_STEAMVR
		Graphics4::setMatrix(vLocation, state.pose.vrPose.eye);
		Graphics4::setMatrix(pLocation, state.pose.vrPose.projection);
#endif

		// Render world
		Graphics4::setMatrix(mLocation, tiger->M);
		tiger->render(tex);


#else
        
        // projection matrix
        mat4 P = mat4::Perspective(45, (float)width / (float)height, 0.1f, 100);
        
        // view matrix
        vec3 lookAt = playerPosition + vec3(0, 0, -1);
        mat4 V = mat4::lookAt(playerPosition, lookAt, vec3(0, 1, 0));
        V *= mat4::Rotation(globe.x(), globe.y(), globe.z());
        
        Graphics4::setMatrix(vLocation, V);
        Graphics4::setMatrix(pLocation, P);
        
        Graphics4::setColorMask(false, false, false, false);
        Graphics4::setRenderState(Graphics4::DepthWrite, false);
        
        // draw bounding box for each object
        MeshObject** boundingBox = &objects[0];
        while (*boundingBox != nullptr) {
            // set the model matrix
            Graphics4::setMatrix(mLocation, (*boundingBox)->M);
            
            if ((*boundingBox)->useQueries) {
                    (*boundingBox)->renderOcclusionQuery();
            }
            
            ++boundingBox;
        }
        
        Graphics4::setColorMask(true, true, true, true);
        Graphics4::setRenderState(Graphics4::DepthWrite, true);
        
        Graphics4::setBlendingMode(Graphics4::SourceAlpha, Graphics4::BlendingOperation::InverseSourceAlpha);
        Graphics4::setRenderState(Graphics4::BlendingState, true);
        Graphics4::setRenderState(Graphics4::DepthTest, true);
        Graphics4::setRenderState(Graphics4::DepthTestCompare, Graphics4::ZCompareLess);
        Graphics4::setRenderState(Graphics4::DepthWrite, true);
        
        // draw real objects
        int renderCount = 0;
        MeshObject** current = &objects[0];
        while (*current != nullptr) {
            // set the model matrix
            Graphics4::setMatrix(mLocation, (*current)->M);
            
            if ((*current)->occluded) {
                (*current)->render(tex);
                ++renderCount;
            }
            
            ++current;
        }
        renderObjectNum = renderCount;
        
#endif
		Graphics4::end();
		Graphics4::swapBuffers();
    }
	
	void keyDown(KeyCode code, wchar_t character) {
		switch (code)
		{
		case Key_Left:
		case Key_A:
			left = true;
			break;
		case Key_Right:
		case Key_D:
			right = true;
			break;
		case Key_Up:
			up = true;
			break;
		case Key_Down:
			down = true;
			break;
		case Key_W:
			forward = true;
			break;
		case Key_S:
			backward = true;
			break;
		case Key_R:
			initCamera();
#ifdef KORE_VR
			VrInterface::resetHmdPose();
#endif
			break;
		case Key_U:
#ifdef KORE_VR
			sit = !sit;
			if (sit) VrInterface::updateTrackingOrigin(TrackingOrigin::Sit);
			else VrInterface::updateTrackingOrigin(TrackingOrigin::Stand);
			log(Info, sit ? "Sit" : "Stand up");
#endif
			break;
		case Key_L:
			Kore::log(Kore::LogLevel::Info, "Position: (%.2f, %.2f, %.2f) - Rotation: (%.2f, %.2f, %.2f)", playerPosition.x(), playerPosition.y(), playerPosition.z(), globe.x(), globe.y(), globe.z());
			log(Info, "Render Object Count: %i", renderObjectNum);
			log(Info, "pixel %u %u\n", objects[0]->pixelCount, objects[1]->pixelCount);
			break;
		default:
			break;
		}
	}

	void keyUp(KeyCode code, wchar_t character) {
		switch (code)
		{
		case Key_Left:
		case Key_A:
			left = false;
			break;
		case Key_Right:
		case Key_D:
			right = false;
			break;
		case Key_Up:
			up = false;
			break;
		case Key_Down:
			down = false;
			break;
		case Key_W:
			forward = false;
			break;
		case Key_S:
			backward = false;
			break;
		default:
			break;
		}
	}

	void mouseMove(int windowId, int x, int y, int movementX, int movementY) {
		if (rotate) {
			globe.x() += (float)((mousePressX - x) * CAMERA_ROTATION_SPEED);
			globe.z() += (float)((mousePressY - y) * CAMERA_ROTATION_SPEED);
			mousePressX = x;
			mousePressY = y;
		}
	}

	void mousePress(int windowId, int button, int x, int y) {
		rotate = true;
		mousePressX = x;
		mousePressY = y;
	}

	void mouseRelease(int windowId, int button, int x, int y) {
		rotate = false;
	}
    
    void init() {
        FileReader vs("shader.vert");
        FileReader fs("shader.frag");
        vertexShader = new Graphics4::Shader(vs.readAll(), vs.size(), Graphics4::VertexShader);
        fragmentShader = new Graphics4::Shader(fs.readAll(), fs.size(), Graphics4::FragmentShader);
        
        // This defines the structure of your Vertex Buffer
		Graphics4::VertexStructure structure;
        structure.add("pos", Graphics4::Float3VertexData);
        structure.add("tex", Graphics4::Float2VertexData);
        structure.add("nor", Graphics4::Float3VertexData);
        
        pipeline = new Graphics4::PipelineState;
		pipeline->inputLayout[0] = &structure;
		pipeline->inputLayout[1] = nullptr;
        pipeline->vertexShader = vertexShader;
        pipeline->fragmentShader = fragmentShader;
		pipeline->depthMode = Graphics4::ZCompareLess;
		pipeline->depthWrite = true;
		pipeline->compile();
        
        tex = pipeline->getTextureUnit("tex");
        
		pLocation = pipeline->getConstantLocation("P");
        vLocation = pipeline->getConstantLocation("V");
        mLocation = pipeline->getConstantLocation("M");
        
		objects[0] = new MeshObject("earth.obj", "earth.png", structure, 1.0f);
		objects[0]->M = mat4::Translation(10.0f, 0.0f, 0.0f);
		objects[1] = new MeshObject("earth.obj", "earth.png", structure, 3.0f);
		objects[1]->M = mat4::Translation(-10.0f, 0.0f, 0.0f);

		tiger = new MeshObject("tiger.obj", "tigeratlas.jpg", structure);
		tiger->M = mat4::Translation(0.0, 0.0, -5.0);
        
        Graphics4::setTextureAddressing(tex, Graphics4::U, Graphics4::Repeat);
        Graphics4::setTextureAddressing(tex, Graphics4::V, Graphics4::Repeat);

		VrInterface::init(nullptr, nullptr, nullptr); // TODO: Remove
    }
}

int kore(int argc, char** argv) {
    System::init("Game", width, height);
    
    init();
    
    Kore::System::setCallback(update);
    
    startTime = System::time();
    
    Keyboard::the()->KeyDown = keyDown;
    Keyboard::the()->KeyUp = keyUp;
	Mouse::the()->Move = mouseMove;
	Mouse::the()->Press = mousePress;
	Mouse::the()->Release = mouseRelease;
    
	initCamera();

    Kore::System::start();
    
    return 0;
}
