#include "black_scholes.h"

#include <cmath>
#include <limits>

namespace asymmetric_leap {
namespace {

constexpr double sqrt_two = 1.4142135623730950488;
constexpr double sqrt_two_pi = 2.5066282746310005024;

double normal_cdf(double value) {
    return 0.5 * std::erfc(-value / sqrt_two);
}

double normal_pdf(double value) {
    return std::exp(-0.5 * value * value) / sqrt_two_pi;
}

bool valid_inputs(double spot, double strike, double years_to_expiry,
                  double volatility) {
    return spot > 0.0 && strike > 0.0 && years_to_expiry > 0.0 &&
           volatility > 0.0 && std::isfinite(spot) && std::isfinite(strike) &&
           std::isfinite(years_to_expiry) && std::isfinite(volatility);
}

double d1_value(double spot, double strike, double years_to_expiry,
                double risk_free_rate, double volatility) {
    return (std::log(spot / strike) +
            (risk_free_rate + 0.5 * volatility * volatility) * years_to_expiry) /
           (volatility * std::sqrt(years_to_expiry));
}

} // namespace

double european_put_price(double spot, double strike, double years_to_expiry,
                          double risk_free_rate, double volatility) {
    if (!valid_inputs(spot, strike, years_to_expiry, volatility)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double root_time = std::sqrt(years_to_expiry);
    const double d1 = d1_value(spot, strike, years_to_expiry, risk_free_rate,
                               volatility);
    const double d2 = d1 - volatility * root_time;
    return strike * std::exp(-risk_free_rate * years_to_expiry) * normal_cdf(-d2) -
           spot * normal_cdf(-d1);
}

double european_put_delta(double spot, double strike, double years_to_expiry,
                          double risk_free_rate, double volatility) {
    if (!valid_inputs(spot, strike, years_to_expiry, volatility)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const double d1 = d1_value(spot, strike, years_to_expiry, risk_free_rate,
                               volatility);
    return normal_cdf(d1) - 1.0;
}

std::optional<double> implied_volatility(double market_price, double spot,
                                         double strike, double years_to_expiry,
                                         double risk_free_rate,
                                         double initial_volatility,
                                         int max_iterations,
                                         double price_tolerance) {
    if (!valid_inputs(spot, strike, years_to_expiry, initial_volatility) ||
        market_price < 0.0 || max_iterations <= 0 || price_tolerance <= 0.0 ||
        !std::isfinite(market_price)) {
        return std::nullopt;
    }

    const double discounted_strike = strike * std::exp(-risk_free_rate * years_to_expiry);
    if (market_price < std::max(0.0, discounted_strike - spot) ||
        market_price >= discounted_strike) {
        return std::nullopt;
    }

    double volatility = initial_volatility;
    for (int iteration = 0; iteration < max_iterations; ++iteration) {
        const double model_price = european_put_price(
            spot, strike, years_to_expiry, risk_free_rate, volatility);
        const double difference = model_price - market_price;
        if (std::abs(difference) <= price_tolerance) {
            return volatility;
        }

        const double d1 = d1_value(spot, strike, years_to_expiry,
                                   risk_free_rate, volatility);
        const double vega = spot * normal_pdf(d1) * std::sqrt(years_to_expiry);
        if (vega <= 1e-12 || !std::isfinite(vega)) {
            return std::nullopt;
        }

        const double next_volatility = volatility - difference / vega;
        if (!std::isfinite(next_volatility) || next_volatility <= 1e-6 ||
            next_volatility > 5.0) {
            return std::nullopt;
        }
        volatility = next_volatility;
    }
    return std::nullopt;
}

double annualized_historical_volatility(const std::vector<double>& closing_prices,
                                        double periods_per_year) {
    if (closing_prices.size() < 3 || periods_per_year <= 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    std::vector<double> returns;
    returns.reserve(closing_prices.size() - 1);
    for (std::size_t index = 1; index < closing_prices.size(); ++index) {
        if (closing_prices[index - 1] <= 0.0 || closing_prices[index] <= 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        returns.push_back(std::log(closing_prices[index] / closing_prices[index - 1]));
    }

    double mean = 0.0;
    for (const double value : returns) {
        mean += value;
    }
    mean /= static_cast<double>(returns.size());

    double squared_deviations = 0.0;
    for (const double value : returns) {
        const double deviation = value - mean;
        squared_deviations += deviation * deviation;
    }
    const double sample_variance = squared_deviations /
                                   static_cast<double>(returns.size() - 1);
    return std::sqrt(sample_variance * periods_per_year);
}

} // namespace asymmetric_leap