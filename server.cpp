
#include "Connection.hpp"

#include "hex_dump.hpp"
#include <glm/glm.hpp>

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
	constexpr float ServerTick = 1.0f / 10.0f; //TODO: set a server tick that makes sense for your game

	//server state:

	//per-client state:
	struct PlayerInfo {
		PlayerInfo() {
			static uint32_t next_player_id = 1;
			name = "Player" + std::to_string(next_player_id);
			next_player_id += 1;
			position = glm::vec3(-7,-1,0);;
			rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		}
		std::string name;

		glm::vec3 position;
		glm::quat rotation;
	};
	std::unordered_map< Connection *, PlayerInfo > players;

	while (true) {
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
					while (c->recv_buffer.size() >= 9) {
						// expecting 9-byte messages 'b' (1 + 1x4 + 4)
						char type = c->recv_buffer[0];
						if (type != 'b') {
							std::cout << " message of non-'b' type received from client!" << std::endl;
							//shut down client connection:
							c->close();
							return;
						}
						// 4 boold
						bool left_pressed = c->recv_buffer[1];
						bool right_pressed = c->recv_buffer[2];
						bool down_pressed = c->recv_buffer[3];
						bool up_pressed = c->recv_buffer[4];
						// 1 float, as 4 bytes
						union float_as_byte
						{
								float   f;
								unsigned char bytes[4];
						} mouse_x;
						mouse_x.bytes[0] = c->recv_buffer[5];
						mouse_x.bytes[1] = c->recv_buffer[6];
						mouse_x.bytes[2] = c->recv_buffer[7];
						mouse_x.bytes[3] = c->recv_buffer[8];
						//std::cout << "x: " + std::to_string(mouse_x.f) <<  "\n";
						// erase 9 bytes
						c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 9);
					}
				}
			}, remain);
		}

		//update current game state
		//TODO: replace with *your* game state update
		std::string status_message = "";
		int32_t overall_sum = 0;
		for (auto &[c, player] : players) {
			std::cout << "socket: " << c->socket << std::endl;
			// (void)c; //work around "unused variable" warning on whatever version of g++ github actions is running
			// for (; player.left_presses > 0; --player.left_presses) {
			// 	player.total -= 1;
			// }
			// for (; player.right_presses > 0; --player.right_presses) {
			// 	player.total += 1;
			// }
			// for (; player.down_presses > 0; --player.down_presses) {
			// 	player.total -= 10;
			// }
			// for (; player.up_presses > 0; --player.up_presses) {
			// 	player.total += 10;
			// }
			// if (status_message != "") status_message += " + ";
			// status_message += std::to_string(player.total) + " (" + player.name + ")";

			// overall_sum += player.total;
		}
		status_message += " = " + std::to_string(overall_sum);
		//std::cout << status_message << std::endl; //DEBUG

		//send updated game state to all clients
		//TODO: update for your game state
		for (auto &[c, player] : players) {
			(void)player; //work around "unused variable" warning on whatever g++ github actions uses
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
