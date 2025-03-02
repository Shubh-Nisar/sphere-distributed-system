#include <iostream>
#include <fstream>
#include <curl/curl.h>
#include <string>
#include <sstream>
#include <memory>

// Function to execute shell commands and capture output
std::string execute_command(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    
    if (!pipe) {
        std::cerr << "Error: Failed to execute command: " << cmd << std::endl;
        return "";
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Remove trailing newline (if present)
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    return result;
}

// Callback function to handle response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

// Function to get Kubernetes token using kubectl
std::string get_k8s_token() {
    std::cout << "Fetching Kubernetes API token...\n";
    std::string token = execute_command("kubectl create token metrics-reader -n kube-system 2>/dev/null");

    if (token.empty()) {
        std::cerr << "Error: Failed to retrieve Kubernetes token. Ensure kubectl is set up correctly.\n";
    }

    return token;
}

// Function to get metrics from Kubernetes Metrics API
std::string fetch_metrics(const std::string& api_url, const std::string& token) {
    CURL* curl;
    CURLcode res;
    std::string response;
    struct curl_slist *headers = NULL;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // Add Authorization Header
        std::string auth_header = "Authorization: Bearer " + token;
        headers = curl_slist_append(headers, auth_header.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Ignore SSL verification (useful for local Minikube testing)
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        // Perform request
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        // Cleanup
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }

    return response;
}

int main() {
    std::string token = get_k8s_token();
    if (token.empty()) {
        return 1;
    }

    // Update with your Kubernetes cluster API server URL
    std::string node_metrics_url = "https://localhost:8443/apis/metrics.k8s.io/v1beta1/nodes";
    std::string pod_metrics_url = "https://localhost:8443/apis/metrics.k8s.io/v1beta1/pods";

    std::cout << "Fetching Node Metrics...\n";
    std::string node_metrics = fetch_metrics(node_metrics_url, token);
    std::cout << node_metrics << std::endl;

    std::cout << "Fetching Pod Metrics...\n";
    std::string pod_metrics = fetch_metrics(pod_metrics_url, token);
    std::cout << pod_metrics << std::endl;

    return 0;
}

