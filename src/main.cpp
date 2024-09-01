// name: main.cpp
// desc: The entry point of our engine/application. Completes the entire lifetime of Fish.
// auth: Luke Hibbert

#include <vk_engine.h>

int main(int argc, char* argv[])
{
	FishEngine engine;

	engine.init();	
	
	engine.run();	

	engine.cleanup();	

	return 0;
}
