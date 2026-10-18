#pragma once
#include <ftxui/component/component.hpp>

class ILibraryService;
class CommandDispatcher;

ftxui::Component CreateUserInputs(ILibraryService& service, CommandDispatcher& dispatcher);
