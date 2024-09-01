// name: main.cpp
// desc: The entry point of our engine/application. Completes the entire lifetime of Fish.
// auth: Luke Hibbert

#include <vk_engine.h>
#include <fish_logger.h>

int main(int argc, char* argv[])
{
	FISH_LOG("----- BOOTING FISH ENGINE -----");

	FishEngine engine;

	engine.init();	
	
	engine.run();	

	engine.cleanup();	

	//FISH_LOG("----- CLOSING FISH ENGINE -----");

	return 0;
}
