# (TODO: your game's title)

### Author: Wenxuan(Neal) Huang, Chunan(Oscar) Huang

### Design: (TODO: In two sentences or fewer, describe what is new and interesting about your game.)

### Networking:  (TODO: How does your game implement client/server multiplayer? What messages are transmitted? Where in the code?)
The network related codes are in the main function of Server.cpp and the update function of Playmode.cpp. <br>
The client handles player's inputs and translates them into infos like position, rotation, and velocity. And then the client will send these infos to the server. <br>
The server takes all players'(clients') infos and updates the game state as well as handles interactions between players. <br>
We also use several Unions (in Unions.hpp) to help on transitting messages that are larger then a byte, so that conversions between any data type (like vec3) and bytes can be easily done.

### Screen Shot:

![Screen Shot](screenshot.png)

### How To Play:

(TODO: describe the controls and (if needed) goals/strategy.)

### Sources: 
(TODO: list a source URL for any assets you did not create yourself. Make sure you have a license for the asset.)
#### Fonts
https://www.fontspace.com/open-sans-font-f22353 <br>
https://www.fontspace.com/serat-font-f57208
#### 3d models

#### Sounds



This game was built with [NEST](NEST.md).

