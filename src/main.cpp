#include "black_scholes.h"
#include "ibkr_bridge.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct ScannerConfig {
    std::string host{"127.0.0.1"};
    int port{7497};
    int client_id{42};
    std::vector<std::string> symbols{"CACC", "CPSS", "CRMT"};
    int minimum_dte{365};
    double maximum_put_delta{-0.20};
    double minimum_asymmetry_ratio{1.2};
    double risk_free_rate{0.04};
};

std::string read_file(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        return {};
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string capture(const std::string& text, const std::string& expression,
                    const std::string& fallback) {
    std::smatch match;
    if (std::regex_search(text, match, std::regex(expression))) {
        return match[1].str();
    }
    return fallback;
}

ScannerConfig load_config(const std::string& path) {
    ScannerConfig config;
    const std::string json = read_file(path);
    if (json.empty()) {
        std::cerr << "Config not found at " << path
                  << "; using built-in scanner defaults.\n";
        return config;
    }

    config.host = capture(json, R"json("host"\s*:\s*"([^"]+)")json", config.host);
    config.port = std::stoi(capture(json, R"("port"\s*:\s*(\d+))",
                                    std::to_string(config.port)));
    config.client_id = std::stoi(capture(json, R"("client_id"\s*:\s*(\d+))",
                                         std::to_string(config.client_id)));
    config.minimum_dte = std::stoi(capture(json, R"("minimum_dte"\s*:\s*(\d+))",
                                          std::to_string(config.minimum_dte)));
    config.maximum_put_delta = std::stod(capture(
        json, R"("maximum_put_delta"\s*:\s*(-?\d+(?:\.\d+)?))",
        std::to_string(config.maximum_put_delta)));
    config.minimum_asymmetry_ratio = std::stod(capture(
        json, R"("minimum_asymmetry_ratio"\s*:\s*(\d+(?:\.\d+)?))",
        std::to_string(config.minimum_asymmetry_ratio)));
    config.risk_free_rate = std::stod(capture(
        json, R"("risk_free_rate"\s*:\s*(\d+(?:\.\d+)?))",
        std::to_string(config.risk_free_rate)));

    std::smatch symbols_match;
    if (std::regex_search(json, symbols_match,
                          std::regex(R"("symbols"\s*:\s*\[([^\]]*)\])"))) {
        config.symbols.clear();
        const std::string symbols_text = symbols_match[1].str();
        const std::regex symbol_expression(R"json("([A-Za-z0-9.-]+)")json");
        for (std::sregex_iterator iterator(symbols_text.begin(), symbols_text.end(),
                                          symbol_expression), end;
             iterator != end; ++iterator) {
            config.symbols.push_back((*iterator)[1].str());
        }
    }
    if (config.symbols.empty()) {
        throw std::runtime_error("Scanner config must include at least one symbol");
    }
    return config;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::string config_path = argc > 1 ? argv[1] : "config/app_config.json";
        const ScannerConfig config = load_config(config_path);

        asymmetric_leap::MockIBKRMarketDataProvider provider;
        provider.connect(config.host, config.port, config.client_id);
        const auto quotes = provider.fetch_put_candidates(config.symbols);
        provider.disconnect();

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Asymmetric LEAP Put Scanner (mock IBKR data)\n"
                  << "Symbol  DTE  Spot   Strike  Put Price  Put Delta  IV      HV      HV/IV   Signal\n";

        for (const auto& quote : quotes) {
            if (quote.days_to_expiry <= config.minimum_dte) {
                continue;
            }

            const double years = static_cast<double>(quote.days_to_expiry) / 365.0;
            const auto iv = asymmetric_leap::implied_volatility(
                quote.market_price, quote.spot, quote.strike, years,
                config.risk_free_rate);
            if (!iv) {
                std::cerr << "Skipping " << quote.symbol
                          << ": unable to solve implied volatility\n";
                continue;
            }

            const double delta = asymmetric_leap::european_put_delta(
                quote.spot, quote.strike, years, config.risk_free_rate, *iv);
            const double historical_volatility =
                asymmetric_leap::annualized_historical_volatility(quote.historical_closes);
            const double asymmetry_ratio = historical_volatility / *iv;
            const bool delta_eligible = delta < 0.0 && delta > config.maximum_put_delta;
            const bool mispriced = asymmetry_ratio > config.minimum_asymmetry_ratio;
            const std::string signal = delta_eligible && mispriced
                                           ? "TAIL-RISK OPPORTUNITY"
                                           : "-";

            std::cout << std::left << std::setw(7) << quote.symbol << std::right
                      << std::setw(5) << quote.days_to_expiry
                      << std::setw(7) << quote.spot
                      << std::setw(9) << quote.strike
                      << std::setw(11) << quote.market_price
                      << std::setw(11) << delta
                      << std::setw(8) << *iv
                      << std::setw(8) << historical_volatility
                      << std::setw(8) << asymmetry_ratio
                      << "   " << signal << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Scanner error: " << error.what() << '\n';
        return 1;
    }
}