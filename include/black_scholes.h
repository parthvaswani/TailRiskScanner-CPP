#pragma once

#include <optional>
#include <vector>

namespace asymmetric_leap {

double european_put_price(double spot, double strike, double years_to_expiry,
                          double risk_free_rate, double volatility);
double european_put_delta(double spot, double strike, double years_to_expiry,
                          double risk_free_rate, double volatility);
std::optional<double> implied_volatility(double market_price, double spot,
                                         double strike, double years_to_expiry,
                                         double risk_free_rate,
                                         double initial_volatility = 0.30,
                                         int max_iterations = 100,
                                         double price_tolerance = 1e-8);
double annualized_historical_volatility(const std::vector<double>& closing_prices,
                                        double periods_per_year = 252.0);

} // namespace asymmetric_leap