#include "PlayMode.hpp"
#include "LitColorTextureProgram.hpp"
#include "DrawLines.hpp"
#include "gl_errors.hpp"
#include "Mesh.hpp"
#include "data_path.hpp"
#include "Load.hpp"
#include "hex_dump.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <random>

GLuint stage_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > stage_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("roller.pnct"));
	stage_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > stage_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("roller.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = stage_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = stage_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > cat_meow_sample(LoadTagDefault, []() -> Sound::Sample const* {
	return new Sound::Sample(data_path("Cat-Meow.opus"));
	});

PlayMode::PlayMode(Client &client_) : client(client_),scene(*stage_scene) {

	// get the transforms of all players' models 
	for (auto &transform : scene.transforms) {
		if (transform.name == "player1") {
			players_transform[0] = &transform;
		}
		else if (transform.name == "player2")
			players_transform[1] = &transform;
		else if (transform.name == "player3")
			players_transform[2] = &transform;
		else if (transform.name == "player4")
			players_transform[3] = &transform;
		else if (transform.name == "Cat")
			cat = &transform;
	}
	// disable other players' drawing, util recieve other players' info from server
	for (size_t i =0; i < players_transform.size(); i++)
		players_transform[i]->draw = false;

	//create a player transform:
	scene.transforms.emplace_back();
	player.transform = &scene.transforms.back();

	//create a player camera attached to a child of the player transform:
	scene.transforms.emplace_back();
	scene.cameras.emplace_back(&scene.transforms.back());
	player.camera = &scene.cameras.back();
	player.camera->fovy = glm::radians(70.0f);
	player.camera->near = 0.01f;
	player.camera->transform->parent = player.transform;

	// cam offset
	player.camera->transform->position = cameraOffset;

	//rotate camera facing direction (-z) to player facing direction (+y):
	player.camera->transform->rotation = glm::angleAxis(glm::radians(20.0f), glm::vec3(0.0f, 0.0f, 1.0f)) * glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	// font
	hintFont = std::make_shared<TextRenderer>(data_path("OpenSans-B9K8.ttf"));
	messageFont = std::make_shared<TextRenderer>(data_path("SeratUltra-1GE24.ttf"));
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	// wasd
	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		}
		if (evt.key.repeat) {
			//ignore repeats
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	}
	
	// mouse
	if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			player.mouse_x = evt.motion.xrel / float(window_size.y),
			player.mouse_y = -evt.motion.yrel / float(window_size.y);

			glm::vec3 up = glm::vec3(0,0,1);
			player.transform->rotation = glm::angleAxis(-player.mouse_x * player.camera->fovy, up) * player.transform->rotation;

			//float pitch = glm::pitch(player.camera->transform->rotation);
			//pitch += player.mouse_y * player.camera->fovy;
			//camera looks down -z (basically at the player's feet) when pitch is at zero.
			//pitch = std::min(pitch, 0.95f * 3.1415926f);
			//pitch = std::max(pitch, 0.05f * 3.1415926f);
			//player.camera->transform->rotation = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));

			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	// update my own transform locally
	{
		//combine inputs into a force:
		glm::vec2 force = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) force.x =-1.0f;
		if (!left.pressed && right.pressed) force.x = 1.0f;
		if (down.pressed && !up.pressed) force.y =-1.0f;
		if (!down.pressed && up.pressed) force.y = 1.0f;
		// update velocity
		glm::vec3 newVelocity = glm::vec3(0.0f);
		// apllying force (has input)
		if (force != glm::vec2(0.0f)) {
			force = glm::normalize(force);
			
			glm::vec3 rotatedForce = player.transform->make_local_to_world() * glm::vec4(-force.y, 0.0f, -force.x, 0.0f);
			// new velocity
			newVelocity = curVelocity + rotatedForce * acceleration * elapsed;
			//newVelocity = curVelocity + force * acceleration * elapsed;
		}
		// no force, use firction
		else{
			if (curVelocity != glm::vec3(0.0f)){
				newVelocity = curVelocity - glm::normalize(curVelocity) * friction * elapsed;
				// make sure frictiuon does not change the direction
				if ((curVelocity.x>0 && newVelocity.x <0) || (curVelocity.x<0 && newVelocity.x >0))
					newVelocity.x = 0;
				if ((curVelocity.y>0 && newVelocity.y <0) || (curVelocity.y<0 && newVelocity.y >0)) 
					newVelocity.y = 0;
			}
		}
		// movement
		glm::vec2 move = (newVelocity + curVelocity) / 2.0f * elapsed;
		curVelocity = newVelocity;
		// update position
		player.transform->position += glm::vec3(move.x, move.y, 0.0f);

		//fall off stage
		{
			if (
				player.transform->position.x < -stageRange.x ||
				player.transform->position.x > stageRange.x ||
				player.transform->position.y < -stageRange.y ||
				player.transform->position.y > stageRange.y
			) {
				player.transform->position = playerInitPos + playerInitPosDistance * (float)(player.id - 1);
				player.transform->rotation = playerInitRot;
				curVelocity = glm::vec3(0.0f);
			}
		}

		// update my model's transform, if i know which model is mine
		if (player.id != 0) {
			players_transform[player.id - 1]->position = player.transform->position;
			players_transform[player.id - 1]->rotation = player.transform->rotation;
			players_velocity[player.id - 1] = curVelocity;
		}
	}

	// sending my info (position , rotation) to server:
	{
		// type 'b' message : char + vec3 + quat
		// char
		client.connections.back().send('b');
		// position
		vec3_as_byte position_bytes;
		position_bytes.vec3_value = player.transform->position;
		for (size_t i =0; i < sizeof(glm::vec3); i++){
			client.connections.back().send(position_bytes.bytes_value[i]);
		}
		// rotation
		quat_as_byte rotation_bytes;
		rotation_bytes.quat_value = player.transform->rotation;
		for (size_t i =0; i < sizeof(glm::quat); i++){
			client.connections.back().send(rotation_bytes.bytes_value[i]);
		}
		// velocity
		vec3_as_byte velocity_bytes;
		velocity_bytes.vec3_value = curVelocity;
		for (size_t i = 0; i < sizeof(glm::vec3); i++) {
			client.connections.back().send(velocity_bytes.bytes_value[i]);
		}
	}

	//send/receive data:
	client.poll([this](Connection *c, Connection::Event event){
		if (event == Connection::OnOpen) {
			std::cout << "[" << c->socket << "] opened" << std::endl;
		} else if (event == Connection::OnClose) {
			std::cout << "[" << c->socket << "] closed (!)" << std::endl;
			throw std::runtime_error("Lost connection to server!");
		} else { assert(event == Connection::OnRecv);
			//std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush();
			//expecting message(s) like 'm' + 3-byte length + length bytes of text:
			while (c->recv_buffer.size() >= 4) {
				//std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush();
				char type = c->recv_buffer[0];
				if (type != 'm') {
					throw std::runtime_error("Server sent unknown message type '" + std::to_string(type) + "'");
				}
				uint32_t size = (
					(uint32_t(c->recv_buffer[1]) << 16) | (uint32_t(c->recv_buffer[2]) << 8) | (uint32_t(c->recv_buffer[3]))
				);
				if (c->recv_buffer.size() < 4 + size) break; //if whole message isn't here, can't process
				//whole message *is* here, so set current server message:
				server_message = std::string(c->recv_buffer.begin() + 4, c->recv_buffer.begin() + 4 + size);

				//and consume this part of the buffer:
				c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 4 + size);
			}
		}
	}, 0.0);

	// update local state according to the server's message
	{
		size_t i_offset = 2; // message after this contains players' individual infos

		// read the public stuff before offset. These are infos same for all players.
		bool serverPlaySound = (bool)server_message[0];
		player_win = (uint8_t)server_message[1];

		// read player's info one by one
		for(size_t i =i_offset; i < server_message.size(); ){

			// read the size of one client's info
			uint32_t size = (
				(uint32_t(server_message[i]) << 16) | (uint32_t(server_message[i+1]) << 8) | (uint32_t(server_message[i+2]))
			);
			// get content of this player
			std::string content = server_message.substr(i+3, size);

			// decode content 
			// id
			uint8_t id = (uint8_t)content[0];
			// position
			vec3_as_byte position;
			for (size_t j =0; j < sizeof(glm::vec3); j++){
				position.bytes_value[j] = content[j+1];
			}
			// rotation
			quat_as_byte rotation;
			for (size_t j =0; j < sizeof(glm::quat); j++){
				rotation.bytes_value[j] = content[j+1+sizeof(glm::vec3)];
			}
			// velocity
			vec3_as_byte velocity;
			for (size_t j = 0; j < sizeof(glm::vec3); j++) {
				velocity.bytes_value[j] = content[j + 1 + sizeof(glm::vec3) + sizeof(glm::quat)];
			}

			// is this my info ? (server will put my own info at first)
			if(i == i_offset){
				// if unkonwn before
				if(player.id == 0){
					player.id = id;
					// set my init position and rotation accroding to my id
					player.transform->position = playerInitPos + playerInitPosDistance * (float)(id-1);
					player.transform->rotation = playerInitRot;
					// enable my own model's drawing (delete if want to disable)
					players_transform[id-1]->draw = true;
				}
			}
			// other players' info
			else{
				players_transform[id-1]->draw = true;
				players_transform[id-1]->position = position.vec3_value;
				players_transform[id-1]->rotation = rotation.quat_value;
				players_velocity[id-1] = velocity.vec3_value;
			}

			// move to next player's info
			i += 3 + size;
		}

		//collide with other player
		{
			for (int id = 1; id <= 4; id++) {
				// check with other online player
				if (id != player.id && players_transform[id - 1]->draw) {

					glm::vec3 diff_vec = player.transform->position - players_transform[id - 1]->position;
					float distance = length(diff_vec) - 2 * playerRange;

					if (distance <= 0) {
						// get player velocity
						glm::vec3 velA = curVelocity;
						glm::vec3 velB = players_velocity[id - 1];

						//reset ball position
						glm::vec3 diff_pos = (-distance) * glm::normalize(diff_vec);
						player.transform->position += diff_pos;

						//hit direction
						glm::vec3 hit_normal = glm::normalize(diff_vec);

						//Seperate ball velocity direction
						glm::vec3 velA_normal = glm::dot(velA, hit_normal) * hit_normal;
						glm::vec3 velA_tangent = velA - velA_normal;
						glm::vec3 velB_normal = glm::dot(velB, hit_normal) * hit_normal;
						glm::vec3 velB_tangent = velB - velB_normal;

						//exchange velocity
						curVelocity = velB_normal + velA_tangent;
					}
				}
			}
		}

		// game logic update
		// play sound
		if(!playingSound && serverPlaySound) {
			Sound::play_3D(*cat_meow_sample, 1, glm::vec3(0));
			playingSound = true;
			startCountDown = true;
		}

		// count down
		if(startCountDown){
			static float countDownCount = 0;
			countDownCount += elapsed;
			if (countDownCount < countDownTime / 3) {
				cat_degree += 90 * elapsed;
			}
			else if (countDownCount < countDownTime / 3 * 2) {
				canMove = false;
			} 
			else if(countDownCount < countDownTime){
				canMove = true;
				cat_degree += 90 * elapsed;
			}
			else {
				countDownCount = 0;
				startCountDown = false;
				playingSound = false;
			}
		}

		cat->rotation = glm::angleAxis(glm::radians(cat_degree), glm::vec3(0.0f, 0.0f, 1.0f));

		if (!canMove && glm::length(curVelocity) > 0.1f) {
			player.transform->position = playerInitPos + playerInitPosDistance * (float)(player.id - 1);
			player.transform->rotation = playerInitRot;
			curVelocity = glm::vec3(0.0f);
		}
	}

}

void PlayMode::draw(glm::uvec2 const &drawable_size) {

	//update camera aspect ratio for drawable:
	player.camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*player.camera);

	// text
	if (player_win == 0)
		hintFont->draw("Your Are Player " + std::to_string(player.id), 20.0f, 550.0f, glm::vec2(0.2,0.25), glm::vec3(0.2, 0.8f, 0.2f));
	else if (player_win == player.id){
		hintFont->draw("Your Win!", 20.0f, 550.0f, glm::vec2(0.2, 0.25), glm::vec3(0.2, 0.8f, 0.2f));
	}
	else {
		hintFont->draw("Your Lose!", 20.0f, 550.0f, glm::vec2(0.2, 0.25), glm::vec3(0.2, 0.8f, 0.2f));
	}

	if(canMove){
		messageFont->draw("Move Now", 20.0f, 500.0f, glm::vec2(0.2,0.25), glm::vec3(0.2, 0.8f, 0.2f));
	}
	else{
		messageFont->draw("STOP !!!", 20.0f, 500.0f, glm::vec2(0.2,0.25), glm::vec3(1.0f, 0.2f, 0.2f));
	}

	GL_ERRORS();
}
