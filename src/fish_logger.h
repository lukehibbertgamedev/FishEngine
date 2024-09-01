
#pragma once

// Macro to print log messages in different colors based on severity
#define FISH_LOG(msg) fmt::println("\033[32m{}\033[0m", msg)    
#define FISH_WARN(msg) fmt::println("\033[33m{}\033[0m", msg)   
#define FISH_FATAL(msg) fmt::println("\033[31m{}\033[0m", msg)  