#include <src/cli/screens.h>

#include <src/cli/input.h>
#include <src/cli/menu.h>
#include <src/cli/utils.h>

#include <src/crypto/mnemonic.h>
#include <src/crypto/utils.h>

#include <curses.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <optional>
#include <vector>

std::unique_ptr<Screen> StartScreen::run() {
	clear();

	mvaddstr(0, 0, "Deterministic password manager");

	std::vector<BasicMenuEntry> start_screen_menu_entries = {
		{ "Import keychain" },
		{ "Create new keychain" },
		{ "Exit" },
	};

	BasicMenu start_screen_menu = BasicMenu({ 3, 5 }, start_screen_menu_entries);
	size_t selected = start_screen_menu.get_user_selection();

	std::vector<std::function<std::unique_ptr<Screen>()>> next_screen_mapping = {
		[]() -> std::unique_ptr<Screen> { return std::make_unique<ImportKeychainScreen>(); },
		[]() -> std::unique_ptr<Screen> { return std::make_unique<NewKeychainScreen>(); },
		[]() -> std::unique_ptr<Screen> { return nullptr; },
	};

	return next_screen_mapping[selected]();
}

void show_error(const Point& pos, const std::string& msg) {
	move(pos.row, 0);
	clrtoeol();
	mvaddstr(pos.row, pos.col, msg.c_str());
	addstr(" Press any key to continue.");
	noecho();
	getch();
}

// Would also work with std::variant<error, fs::path>
std::optional<std::filesystem::path> get_db_path() {
	StringInputHandler path_input({3, 5}, "Database path (max. 256 chars) [~/.hdpwm]: ");
	input_action_result path_action_result = path_input.process();
	if (path_action_result == input_action_result::BACK) {
		return {};
	}

	std::string path_value = path_input.value.empty() ? "~/.hdpwm" : path_input.value;

	std::filesystem::path path;

	if (path_value.size() > 0 && path_value.substr(0, 1) == "~") {
		path = std::filesystem::path(getenv("HOME")) / path_value.substr(2);
	} else {
		path = path_value;
	}

	if (std::filesystem::exists(path)) {
		show_error({5, 5}, "This file already exists, refusing to delete it.");
		return {};
	} else if (!std::filesystem::is_directory(path.parent_path())) {
		show_error({5, 5}, "The parent directory does not exist, create it first.");
		return {};
	}

	return path;
}

std::optional<utils::sensitive_string> get_password() {
	SensitiveInputHandler password_input({5, 5}, "Password (max 256 chars): ");
	if (password_input.process() == input_action_result::BACK) {
		return {};
	}

	return std::move(password_input.value);
}

std::unique_ptr<Screen> NewKeychainScreen::run() {
	clear();

	mvaddstr(0, 0, "Creating new keychain");
	int maxlines = LINES - 1;
	mvaddstr(maxlines, 0, "<shift>-<left arrow> to go back | <return> to continue");

	std::filesystem::path path;
	if (auto provided_path = get_db_path()) {
		path = provided_path.value();
	} else {
		return std::make_unique<StartScreen>();
	}

	utils::sensitive_string password;
	if (auto provided_password = get_password()) {
		password = std::move(provided_password.value());
	} else {
		return std::make_unique<StartScreen>();
	}

	std::vector<std::string> mnemonic = crypto::generate_mnemonic(24);
	mvaddstr(7, 5, "Please write down the following mnemonic and press any key to continue.");
	move(8, 5);
	for (std::string &word : mnemonic) {
		addstr(word.c_str());
		utils::secure_zero_string(word);
		addch(' ');
	}

	getch();

	// TODO: initialize db with seed/mnemonic

	return std::make_unique<StartScreen>();
}

std::unique_ptr<Screen> ImportKeychainScreen::run() {
	return nullptr;
}
