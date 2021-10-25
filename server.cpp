#include "Connection.hpp"

#include "hex_dump.hpp"
#include <glm/glm.hpp>
#include "Unions.hpp"
#include <chrono>
#include <stdexcept>
#include <iostream>
#include <cassert>
#include <unordered_map>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#ifdef _WIN32
extern "C" { uint32_t GetACP(); }
#endif
int main(int argc, char **argv) {
#ifdef _WIN32
	{ //when compiled on windows, check that code page is forced to utf-8 (makes file loading/saving work right):
		//see: https://docs.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page
		uint32_t code_page = GetACP();
		if (code_page == 65001) {
			std::cout << "Code page is properly set to UTF-8." << std::endl;
		} else {
			std::cout << "WARNING: code page is set to " << code_page << " instead of 65001 (UTF-8). Some file handling functions may fail." << std::endl;
		}
	}

	//when compiled on windows, unhandled exceptions don't have their message printed, which can make debugging simple issues difficult.
	try {
#endif

	//------------ argument parsing ------------

	if (argc != 2) {
		std::cerr << "Usage:\n\t./server <port>" << std::endl;
		return 1;
	}

	//------------ initialization ------------

	Server server(argv[1]); 


	//------------ main loop ------------
	constexpr float ServerTick = 1.0f / 60.0f; // set a server tick that makes sense for your game

	//server state:
	const uint16_t player_amount = 4; // only max 4 players in the game for now
	const float soundWaitTime = 6.0f; // play sound every this much time
	bool playSound = false;

	//per-client state:
	struct PlayerInfo {
		PlayerInfo() {
			static uint8_t next_player_id = 1;
			id = next_player_id;
			next_player_id += 1;
			position = glm::vec3(-7,-1,0);;
			rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			velocity = glm::vec3(0.0f);
		}
		// convert player's info into bytes, with form of string
		std::string toByteString(){
			std::string result = "";
			// id
			result += (char)id;
			// position
			vec3_as_byte position_bytes;
			position_bytes.vec3_value = position;
			for (size_t i =0; i < sizeof(position); i++){
				result += position_bytes.bytes_value[i];
			}
			// rotation
			quat_as_byte rotation_bytes;
			rotation_bytes.quat_value = rotation;
			for (size_t i =0; i < sizeof(rotation); i++){
				result += rotation_bytes.bytes_value[i];
			}

			//velocity
			vec3_as_byte velocity_bytes;
			velocity_bytes.vec3_value = velocity;
			for (size_t i = 0; i < sizeof(velocity); i++) {
				result += velocity_bytes.bytes_value[i];
			}

			// now calculate total string size, and add that to the beginning (using 3 bytes)
			std::string size_string = "";
			size_string += (char)(uint8_t(result.size() >> 16));
			size_string += (char)(uint8_t((result.size() >> 8) % 256));
			size_string += (char)(uint8_t(result.size() % 256));
			// combine
			result = size_string + result;
			return result;
		}
		// info
		uint8_t id;
		glm::vec3 position;
		glm::quat rotation;
		glm::vec3 velocity;
	};
	std::unordered_map< Connection *, PlayerInfo > players;
	players.reserve(player_amount); 

	while (true) {

		// play sound check
		{
			// only start when there are more than 2 players
			if(players.size() >= 2){
				static auto prev_play_time = std::chrono::steady_clock::now();
				auto now = std::chrono::steady_clock::now();
				if(std::chrono::duration< float >(now-prev_play_time).count() >= soundWaitTime){
					prev_play_time = std::chrono::steady_clock::now();
					playSound = true;
				}
				else{
					playSound = false;
				}
			}
		}

		static auto next_tick = std::chrono::steady_clock::now() + std::chrono::duration< double >(ServerTick);
		//process incoming data from clients until a tick has elapsed:
		while (true) {
			auto now = std::chrono::steady_clock::now();
			double remain = std::chrono::duration< double >(next_tick - now).count();
			if (remain < 0.0) {
				next_tick += std::chrono::duration< double >(ServerTick);
				break;
			}
			server.poll([&](Connection *c, Connection::Event evt){
				if (evt == Connection::OnOpen) {
					//client connected:
					//create some player info for them:
					players.emplace(c, PlayerInfo());
					// refuse connection if over size
					if(players.size() > player_amount){
						//shut down client connection:
							c->close();
							return;
					}
				} else if (evt == Connection::OnClose) {
					//client disconnected:

					//remove them from the players list:
					auto f = players.find(c);
					assert(f != players.end());
					players.erase(f);
				} else { assert(evt == Connection::OnRecv);
					//got data from client:
					//std::cout << "got bytes:\n" << hex_dump(c->recv_buffer); std::cout.flush();

					//look up in players list:
					auto f = players.find(c);
					assert(f != players.end());
					PlayerInfo &player = f->second;

					// --------------- handle messages from client ---------------- //
					size_t client_mes_size = 1 + 2 * sizeof(glm::vec3) + sizeof(glm::quat);
					while (c->recv_buffer.size() >= client_mes_size) {
						size_t index = 0;
						// expecting messages 'b' 
						char type = c->recv_buffer[0];
						if (type != 'b') {
							std::cout << " message of non-'b' type received from client!" << std::endl;
							//shut down client connection:
							c->close();
							return;
						}
						index += 1;
						// position
						vec3_as_byte position;
						for (size_t j =0; j < sizeof(glm::vec3); j++){
							position.bytes_value[j] = c->recv_buffer[index+j];
						}
						player.position = position.vec3_value;
						index += sizeof(glm::vec3);
						// rotation
						quat_as_byte rotation;
						for (size_t j =0; j < sizeof(glm::quat); j++){
							rotation.bytes_value[j] = c->recv_buffer[index+j];
						}
						player.rotation = rotation.quat_value;
						index += sizeof(glm::quat);
						// velocity
						vec3_as_byte velocity;
						for (size_t j = 0; j < sizeof(glm::vec3); j++) {
							velocity.bytes_value[j] = c->recv_buffer[index + j];
						}
						player.velocity = velocity.vec3_value;
						index += sizeof(glm::vec3);
						// erase bytes
						c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + client_mes_size);
					}
				}
			}, remain);
		}

		// ----------- send updated game state to all clients -------------- //
		for (auto &[c, player] : players) {

			(void)player; //work around "unused variable" warning on whatever g++ github actions uses
			// construct a status message
			std::string status_message = "";

			// ------- put public message (same for all player) ------- //
			status_message += (char)playSound;

			// ------- put all players' infos -------- //
			// put the info of client itself at first
			auto f = players.find(c);
			assert(f != players.end());
			PlayerInfo &player = f->second;
			status_message += player.toByteString();
			// put the info of other players 
			for (auto &[c_other, player_other] : players){
				if (c != c_other){
					status_message += player_other.toByteString();
				}
			}

			//send an update starting with 'm', a 24-bit size, and a blob of text:
			c->send('m');
			c->send(uint8_t(status_message.size() >> 16));
			c->send(uint8_t((status_message.size() >> 8) % 256));
			c->send(uint8_t(status_message.size() % 256));
			c->send_buffer.insert(c->send_buffer.end(), status_message.begin(), status_message.end());
		}

	}


	return 0;

#ifdef _WIN32
	} catch (std::exception const &e) {
		std::cerr << "Unhandled exception:\n" << e.what() << std::endl;
		return 1;
	} catch (...) {
		std::cerr << "Unhandled exception (unknown type)." << std::endl;
		throw;
	}
#endif
}
