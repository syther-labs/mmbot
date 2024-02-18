#include "strategy_powern.h"
#include "numerical.h"
#include <imtjson/object.h>

//smallest value - default count of steps of find_root is 32, so smallest unit is aprx 1e-10
constexpr double epsilon = 1e-14;

Strategy_PowerN::Strategy_PowerN(Config cfg):_cfg(cfg),_state{} {}
Strategy_PowerN::Strategy_PowerN(Config cfg, State state):_cfg(cfg),_state(state) {}


double Strategy_PowerN::find_k(double w, double c, double p, double price,
        double val, double pos) const {
    if (val >= epsilon) return price;
    return not_nan(Numerics<>::find_root_pos(price, pos,[&](double k){
        return integral_fnx(p, w, k, c, price) - val;
    }),price);
}

double Strategy_PowerN::find_k_from_pos(double w, double c, double p,
        double price, double pos) const {
    if (std::abs(pos) < epsilon) return price;
    return not_nan(Numerics<>::find_root_pos(price, pos,[&](double k){
        return fnx(p, w, k, c, price) - pos;
    }),price);

}

double Strategy_PowerN::fnx(double p, double w, double k, double c, double x) {
    double xk = x/k;
    return (w*p*c)/(2*k*w*w)*(std::pow(xk,-w)-std::pow(xk,w));

}

double Strategy_PowerN::integral_fnx(double p, double w, double k, double c, double x) {
    double xk = x/k;
    return -(c*p*(-2*k*w+(1+w)*x*std::pow(xk,-w)+(w-1)*x*std::pow(xk,w))/(2*k*(w-1)*w*(w+1)));
}

double Strategy_PowerN::invert_fnx(double p, double w, double k, double c,double x) {
    return k*std::pow((std::sqrt(c*c*p*p+k*k*x*x*w*w) -  k*x*w)/(c*p),1.0/w);
}

Strategy_PowerN::RuleResult Strategy_PowerN::find_k_rule(double new_price, bool alert) const {
    double aprx_pnl = _state._pos * (new_price - _state._p);
    double new_val = _state._val + aprx_pnl;
    double new_k = _state._k;
    double yield = calc_value(_cfg, _state._p,new_price);
    if ((_state._p -_state._k) * (new_price - _state._k) < 0) {
            new_k = new_price;
    } else {
        if (aprx_pnl < 0 || alert) {
            new_k = find_k(_cfg, new_price, new_val, _state._pos);
        } else if (new_val < 0 || !_state._pos) {
                double y = _state._pos?_cfg.yield_mult:_cfg.initial_yield_mult;
                double extra_val = yield * y;
                new_val += extra_val;
                new_k = find_k(_cfg, new_price, new_val, _state._pos?_state._pos:(_state._p - new_price));
        }
        if (_state._pos && ((new_k - _state._k) * (new_price - _state._k)< 0)) {
            new_k = _state._k;
        }
    }
    return {
        new_k,
        calc_value(_cfg, new_k, new_price),
        calc_position(_cfg,new_k, new_price)
    };
}

bool Strategy_PowerN::isValid() const {
    return _state._k > 0 && _state._p > 0;
}

PStrategy Strategy_PowerN::importState(json::Value src, const IStockApi::MarketInfo &minfo) const {
    State st = {
            src["val"].getNumber(),
            src["k"].getNumber(),
            src["p"].getNumber(),
            src["pos"].getNumber()
    };
    return PStrategy(new Strategy_PowerN(_cfg, st));
}

json::Value Strategy_PowerN::exportState() const {
    return json::Object{
        {"val", _state._val},
        {"k",_state._k},
        {"p",_state._p},
        {"pos", _state._pos}
    };
}

json::Value Strategy_PowerN::dumpStatePretty(const IStockApi::MarketInfo &minfo) const {
    return json::Object {};
}

PStrategy Strategy_PowerN::init(const IStockApi::MarketInfo &minfo, double price, double assets, double currency) const {
    State st;
    st._k = find_k_from_pos(_cfg, price, assets);
    st._val = calc_value(_cfg, st._k, price);
    st._p = price;
    st._pos = assets;
    PStrategy out(new Strategy_PowerN(_cfg, st));
    if (!out->isValid()) throw std::runtime_error("Unable to initialize strategy");
    return out;
}

IStrategy::OrderData Strategy_PowerN::getNewOrder(
        const IStockApi::MarketInfo &minfo, double cur_price, double new_price,
        double dir, double assets, double currency, bool rej) const {

    if (!Strategy_PowerN::isValid()) return init(minfo, cur_price, assets, currency)->getNewOrder(minfo, cur_price, new_price, dir, assets, currency, rej);

    double ord =calc_order(new_price, dir)*dir;
    return {0, ord, Alert::enabled};

}

std::pair<IStrategy::OnTradeResult, PStrategy> Strategy_PowerN::onTrade(
        const IStockApi::MarketInfo &minfo, double tradePrice, double tradeSize,
        double assetsLeft, double currencyLeft) const {

    if (!Strategy_PowerN::isValid()) return init(minfo, tradePrice, assetsLeft-tradeSize, currencyLeft)
                ->onTrade(minfo, tradePrice, tradeSize, assetsLeft, currencyLeft);

    if (std::abs(assetsLeft) < minfo.calcMinSize(tradePrice)) {
        assetsLeft = 0;
    }
    RuleResult r = find_k_rule(tradePrice,!tradeSize);
    double new_price = tradePrice;
    if (tradeSize) {
        new_price = find_price_from_pos(_cfg, r.k, assetsLeft);
        r = find_k_rule(tradePrice);
    }
    State new_state;
    new_state._val = r.val;
    new_state._k = r.k;
    new_state._p = new_price;
    new_state._pos = assetsLeft;

    double pnl = (tradePrice - _state._p) * (assetsLeft - tradeSize);

    double np = _state._val - r.val + pnl;
    return {
        {np, 0, new_state._k, 0},
        PStrategy(new Strategy_PowerN(_cfg, new_state))
    };


}

PStrategy Strategy_PowerN::onIdle(const IStockApi::MarketInfo &minfo,
        const IStockApi::Ticker &curTicker, double assets,
        double currency) const {
    if (!Strategy_PowerN::isValid()) return init(minfo, curTicker.last, assets, currency)
                ->onIdle(minfo, curTicker, assets, currency);
    return PStrategy(this);
}

PStrategy Strategy_PowerN::reset() const {
    return PStrategy(new Strategy_PowerN(_cfg));
}

double Strategy_PowerN::calcInitialPosition(const IStockApi::MarketInfo &, double , double , double ) const {
    return 0;
}

double Strategy_PowerN::getCenterPrice(double lastPrice, double assets) const {
    return Strategy_PowerN::getEquilibrium(assets);
}

double Strategy_PowerN::getEquilibrium(double assets) const {
    return find_price_from_pos(_cfg, _state._k, assets);
}

IStrategy::MinMax Strategy_PowerN::calcSafeRange(
        const IStockApi::MarketInfo &minfo, double assets,
        double currencies) const {

    if (minfo.leverage) {

        double budget = currencies - _state._val;
        double min_val = Numerics<15>::find_root_to_zero(_state._k, [&](double x){
            return calc_value(_cfg, _state._k, x) + budget;
        });
        double max_val = Numerics<15>::find_root_to_inf(_state._k, [&](double x){
            return calc_value(_cfg, _state._k, x) + budget;
        });
        return {min_val, max_val};
    } else {
        double budget = currencies + assets*_state._p - _state._val;
        double min_val = Numerics<15>::find_root_to_zero(_state._k, [&](double x){
            return calc_value(_cfg, _state._k, x) + calc_position(_cfg, _state._k, x)*x + budget;
        });
        return {min_val, _state._k};
    }

}

double Strategy_PowerN::calcCurrencyAllocation(double price, bool leveraged) const {
    if (leveraged) {
        return calc_value(_cfg, _state._k, price) + _cfg.initial_budget;
    } else {
        return _state._val + _cfg.initial_budget - _state._p * _state._pos;
    }
}

std::string_view Strategy_PowerN::getID() const {
    return Strategy_PowerN::id;
}

IStrategy::BudgetInfo Strategy_PowerN::getBudgetInfo() const {
    return {
        _cfg.initial_budget + _state._val,
        _state._pos
    };
}

IStrategy::ChartPoint Strategy_PowerN::calcChart(double price) const {
    return {
        true,
        calc_position(_cfg, _state._k, price),
        calc_value(_cfg, _state._k, price) + _cfg.initial_budget
    };
}

double Strategy_PowerN::calc_order(double price, double side) const {
    RuleResult r = find_k_rule(price);
    double apos = r.pos * side;
    double diff = apos - _state._pos * side;
    return diff;

}
