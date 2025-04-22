#include "pch.h"

#include "windowbuilder.h"
#include "windowbuilder_imgui.h"

#include "widgets.h"
#include "log.h"

constexpr ImGuiWindowFlags WINDOW_FLAGS = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus;

enum class Format : int {
	MP3,
	MP4
};

static std::optional<std::jthread> downloadThread;
static std::atomic<bool> downloadComplete = true;

void EnsureYtDlp();
std::string RunCommandAndCaptureOutput(const std::string& command) {
    SECURITY_ATTRIBUTES saAttr{};
    saAttr.nLength = sizeof(saAttr);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE hPipeRead, hPipeWrite;
    if (!CreatePipe(&hPipeRead, &hPipeWrite, &saAttr, 0)) {
        spdlog::error("CreatePipe failed, err={}", GetLastError());
        return "";
    }

    SetHandleInformation(hPipeRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hPipeWrite;
    si.hStdError  = hPipeWrite;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(
            NULL,
            const_cast<char*>(command.c_str()),
            NULL, NULL,
            TRUE,
            CREATE_NO_WINDOW,
            NULL, NULL,
            &si, &pi))
    {
        spdlog::error("CreateProcess failed, err={}", GetLastError());
        CloseHandle(hPipeRead);
        CloseHandle(hPipeWrite);
        return "";
    }

    CloseHandle(hPipeWrite);

    std::string output;
    char buffer[512];
    DWORD bytesRead;
    while (ReadFile(hPipeRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
        output.append(buffer, bytesRead);
    }

    CloseHandle(hPipeRead);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
    output.erase(std::remove(output.begin(), output.end(), '\r'), output.end());
    return output;
}

void DownloadThread(bool audioOnly, const std::string& url) {
    downloadComplete = false;
    EnsureYtDlp();

    const std::string outputTemplate = "%(title)s.%(ext)s";

    // Step 1: Get the filename
    std::ostringstream getNameCmd;
    getNameCmd << "yt-dlp.exe --print filename ";
    getNameCmd << "--output \"" << outputTemplate << "\" ";
    getNameCmd << "\"" << url << "\"";

    std::string resultFilename = RunCommandAndCaptureOutput(getNameCmd.str());
    if (resultFilename.empty()) {
        spdlog::error("Could not determine output filename.");
        downloadComplete = true;
        return;
    }

    // Step 2: Download the file
    std::ostringstream downloadCmd;
    downloadCmd << "yt-dlp.exe ";
    if (audioOnly) {
        downloadCmd << "--extract-audio --audio-format mp3 ";
    }
    else {
        downloadCmd << "--format bestvideo+bestaudio/best ";
    }
    downloadCmd << "--merge-output-format mp4 ";
    downloadCmd << "--output \"" << outputTemplate << "\" ";
    downloadCmd << "\"" << url << "\"";

    spdlog::debug("Running command: {}", downloadCmd.str());

    std::string downloadOutput = RunCommandAndCaptureOutput(downloadCmd.str());

    std::filesystem::path filePath(resultFilename);
    if (audioOnly) {
        filePath.replace_extension(".mp3");
    }
    else {
        filePath.replace_extension(".mp4");
    }
    spdlog::info("Download complete: {}", filePath.string());

    downloadComplete = true;
}

void Render(const Window& window) {
	ImGui::SetNextWindowSize({ (float)window.width, (float)window.height });
	ImGui::SetNextWindowPos({ 0, 0 });
	ImGui::Begin("ImDL", nullptr, WINDOW_FLAGS);

	static char buffer[256] = "";
	ImGui::PushItemWidth(-1);
	static bool urlValid = false;
	if (ImGui::InputTextWithHint("##URL", "https://www.youtube.com/watch?v=br3GIIQeefY" ICON_LC_LINK, buffer, sizeof(buffer))) {
		std::regex urlRegex(R"(^(https?://)?(www\.|music\.)?(youtube\.com/watch\?v=|youtu\.be/)[\w-]{11}(&.*)?$)");
		urlValid = std::regex_match(buffer, urlRegex);
	}
	ImGui::Spacing();

	static Format format = Format::MP4;

	ImGui::BeginDisabled(!urlValid || !downloadComplete);
	//if (ImGui::Button(ICON_LC_DOWNLOAD " Download", ImVec2(-1, window.height - 80))) {

	constexpr auto standardText = ICON_LC_DOWNLOAD " Download";
	constexpr auto downloadingText = ICON_LC_CIRCLE_DASHED " Downloading...";

	if (ImGui::Button(!downloadComplete ? downloadingText : standardText, ImVec2(-1, (float)window.height - 80.0f))) {
		spdlog::info("Starting download..");
#
		if (downloadThread && downloadThread->joinable()) {
			downloadThread->join();
		}

		downloadThread = std::jthread(DownloadThread, format == Format::MP3, buffer);
	}

	ImGui::Combo("##Format", (int*)&format, "MP3\0MP4\0\0");
	ImGui::EndDisabled();

	ImGui::End();

	ImGui::ToastSystem::RenderAll();
}

void EnsureYtDlp() {
	if (!std::filesystem::exists("yt-dlp.exe")) {
		spdlog::info("yt-dlp.exe not found, downloading...");
	}
	else {
		spdlog::debug("yt-dlp.exe already exists, skipping download.");
		return;
	}

	constexpr auto url = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe";
	cpr::Response r = cpr::Get(cpr::Url{ url });
	if (r.status_code == 200) {
		std::ofstream out("yt-dlp.exe", std::ios::binary);
		out.write(r.text.c_str(), r.text.size());
		out.close();
	}
	else {
		spdlog::error("Download failed: {}", r.status_code);
	}
}

BOOL APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
	SetupLogger();

#ifdef _DEBUG
	auto res = AllocConsole();
	if (res) {
		freopen_s(reinterpret_cast<FILE**>(stdout), "CONOUT$", "w", stdout);
		freopen_s(reinterpret_cast<FILE**>(stderr), "CONOUT$", "w", stderr);
	}

	spdlog::set_level(spdlog::level::debug);
	spdlog::set_pattern("%^[%T] %n: %v%$");

	spdlog::debug("Debug console allocated");
#endif

	auto window = WindowBuilder()
		.Name("ImDL", "ImDL")
		.Size(600, 150)
		.ImmersiveTitlebar()
		.VSync(true)
		.OnRender(Render)
		.Plugin<WindowBuilderImGui>()
		.Build();

	window->Show();

#ifdef _DEBUG
	spdlog::debug("Debug console freed");
	FreeConsole();
#endif
}