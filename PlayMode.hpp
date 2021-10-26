#include "Mode.hpp"
#include "Scene.hpp"
#include "Sound.hpp"
#include "Connection.hpp"
#include "Unions.hpp"
#include "TextRenderer.hpp"
#include <glm/glm.hpp>

#include <vector>
#include <array>
#include <deque>

struct PlayMode : Mode {
	PlayMode(Client &client);
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;
	// transforms of other players
	std::array <Scene::Transform *, 4> players_transform;
	std::array <glm::vec3, 4> players_velocity;
	Scene::Transform* cat;
	float cat_degree = 180.0f;

	// player info
	struct Player {
		uint8_t id = 0; // 0 for unkonw
		//transform is at player's feet and will be yawed by mouse left/right motion:
		Scene::Transform *transform = nullptr;
		//camera is at player's head and will be pitched by mouse up/down motion:
		Scene::Camera *camera = nullptr;
		float mouse_x;
		float mouse_y;
	} player;

	// game logics
	// players
	const glm::vec3 cameraOffset = glm::vec3(2.0f, 1.5f, 0.0f);
	const glm::vec3 playerInitPos = glm::vec3(3.0f, -19.0f, 0.0f);
	const glm::vec3 playerInitPosDistance = glm::vec3(-2.0f, -0.0f, 0.0f);
	const glm::quat playerInitRot = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f)) * glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	const float acceleration = 1.5f;
	const float friction = 1.0f;

	glm::vec3 curVelocity = glm::vec3(0.0f);

	//stage
	const glm::vec2 stageRange = glm::vec2(4.0f, 20.0f);
	const float playerRange = 0.9f;
	uint8_t player_win = 0;

	// sound
	bool playingSound = false;
	bool startCountDown = false;
	const float countDownTime = 6.0f;
	bool canMove = true;

	//last message from server:
	std::string server_message;

	//connection to server:
	Client &client;

	std::shared_ptr<TextRenderer> hintFont;
	std::shared_ptr<TextRenderer> messageFont;

};
