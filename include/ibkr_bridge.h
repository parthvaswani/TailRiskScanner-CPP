#pragma once

#include "black_scholes.h"

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace asymmetric_leap {

struct OptionQuote {
    std::string symbol;
    double spot{};
    double strike{};
    int days_to_expiry{};
    double market_price{};
    std::vector<double> historical_closes;
};

class MarketDataProvider {
public:
    virtual ~MarketDataProvider() = default;
    virtual std::vector<OptionQuote> fetch_put_candidates(
        const std::vector<std::string>& symbols) = 0;
};

// This boundary keeps the scanner independent of the proprietary IBKR SDK.
// An SDK-backed implementation can translate EWrapper callbacks and
// EClientSocket requests into OptionQuote values here.
class IBKRMarketDataProvider : public MarketDataProvider {
public:
    virtual void connect(const std::string& host, int port, int client_id) = 0;
    virtual void disconnect() = 0;
};

class MockIBKRMarketDataProvider final : public IBKRMarketDataProvider {
public:
    MockIBKRMarketDataProvider() {
        fixtures_.emplace("CACC", Fixture{50.0, 35.0, 730, 0.35, 0.52});
        fixtures_.emplace("CPSS", Fixture{10.0, 6.5, 550, 0.45, 0.42});
        fixtures_.emplace("CRMT", Fixture{40.0, 30.0, 480, 0.40, 0.63});
    }

    void connect(const std::string& host, int port, int client_id) override {
        if (host.empty() || port <= 0 || client_id < 0) {
            throw std::invalid_argument("Invalid mock IBKR connection settings");
        }
        connected_ = true;
    }

    void disconnect() override { connected_ = false; }

    std::vector<OptionQuote> fetch_put_candidates(
        const std::vector<std::string>& symbols) override {
        if (!connected_) {
            throw std::runtime_error("Market data provider is not connected");
        }

        std::vector<OptionQuote> quotes;
        for (const auto& symbol : symbols) {
            const auto fixture = fixtures_.find(symbol);
            if (fixture == fixtures_.end()) {
                continue;
            }

            const auto& data = fixture->second;
            const double years = static_cast<double>(data.days_to_expiry) / 365.0;
            quotes.push_back(OptionQuote{
                symbol,
                data.spot,
                data.strike,
                data.days_to_expiry,
                european_put_price(data.spot, data.strike, years, 0.04,
                                   data.implied_volatility),
                make_history(data.spot, data.historical_volatility)});
        }
        return quotes;
    }

private:
    struct Fixture {
        double spot;
        double strike;
        int days_to_expiry;
        double implied_volatility;
        double historical_volatility;
    };

    static std::vector<double> make_history(double latest_price, double volatility) {
        std::vector<double> prices(31);
        prices.back() = latest_price;
        const double daily_move = volatility / std::sqrt(252.0);
        for (std::size_t index = prices.size() - 1; index > 0; --index) {
            const double return_value = index % 2 == 0 ? daily_move : -daily_move;
            prices[index - 1] = prices[index] / std::exp(return_value);
        }
        return prices;
    }

    bool connected_{false};
    std::unordered_map<std::string, Fixture> fixtures_;
};

} // namespace asymmetric_leap