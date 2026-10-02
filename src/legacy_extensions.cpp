#include "metalarch/legacy_extensions.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <vector>

namespace ma {
namespace {
constexpr int64_t BAR_TTL=120'000'000'000LL;
Result valid(double x){Result r;r.status=Status::Valid;r.value=x;return r;}
Result missing(const char* reason){Result r;r.status=Status::Unavailable;r.reason=reason;return r;}
const EventWindow& history(const ReadView& view,const Key& key){return view.get(key)->events;}
// The nonnegative source window and valid OHLC are enforced by Store::ingest.
}
std::vector<Descriptor> make_legacy_extensions(const Key& primary){
  std::vector<Descriptor> out;
  out.reserve(8);
  // ASEP2 oscillator_bank: percentage close-to-close rate of change, 12 bars.
  out.push_back({"roc12","asep2-roc12-native-v1","lag=12;unit=percent",{primary},{},BAR_TTL,
    [primary](const ReadView& view,const Results&){
      const auto& v=history(view,primary);
      if(v.size()<13)return missing("ROC requires 13 closing observations");
      return valid(100.0*(v.back().d/v[v.size()-13].d-1.0));
    }});
  // ASEP2 oscillator_bank: most recent 14 high/low bars (including current).
  out.push_back({"williams_r14","asep2-williams-r-native-v1","window=14;unit=percent",{primary},{},BAR_TTL,
    [primary](const ReadView& view,const Results&){
      const auto& v=history(view,primary);
      if(v.size()<14)return missing("Williams R requires 14 bars");
      double high=v.back().b,low=v.back().c;
      for(size_t i=v.size()-14;i<v.size();++i){high=std::max(high,v[i].b);low=std::min(low,v[i].c);}
      if(high-low<=1e-14*std::max(1.0,high))return missing("Williams R undefined for zero price range");
      return valid(-100.0*(high-v.back().d)/(high-low));
    }});
  // ASEP2 oscillator_bank: CCI using mean absolute deviation, NOT standard deviation.
  out.push_back({"cci20","asep2-cci-native-v1","window=20;scale=0.015",{primary},{},BAR_TTL,
    [primary](const ReadView& view,const Results&){
      const auto& v=history(view,primary);
      if(v.size()<20)return missing("CCI requires 20 bars");
      double typical[20],mean=0;
      for(size_t j=0;j<20;++j){const auto& e=v[v.size()-20+j];typical[j]=(e.b+e.c+e.d)/3.0;mean+=typical[j];}
      mean/=20.0;
      double mad=0;for(double x:typical)mad+=std::abs(x-mean);mad/=20.0;
      if(mad<=1e-14*std::max(1.0,std::abs(mean)))return missing("CCI undefined for zero mean absolute deviation");
      return valid((typical[19]-mean)/(0.015*mad));
    }});
  // ASEP2 realized_vol: per-bar Parkinson estimator. The old file applies an
  // unconditional annualization multiplier that is unsuitable for arbitrary timeframes.
  out.push_back({"parkinson20","asep2-parkinson-native-v1","window=20;unit=per-bar",{primary},{},BAR_TTL,
    [primary](const ReadView& view,const Results&){
      const auto& v=history(view,primary);
      if(v.size()<20)return missing("Parkinson requires 20 bars");
      double sum=0;
      for(size_t i=v.size()-20;i<v.size();++i){const double z=std::log(v[i].b/v[i].c);sum+=z*z;}
      return valid(std::sqrt(sum/(20.0*4.0*std::numbers::ln2_v<double>)));
    }});
  // ASEP2 realized_vol: Garman-Klass OHLC volatility, also per bar.
  out.push_back({"garman_klass20","asep2-garman-klass-native-v1","window=20;unit=per-bar",{primary},{},BAR_TTL,
    [primary](const ReadView& view,const Results&){
      const auto& v=history(view,primary);
      if(v.size()<20)return missing("Garman-Klass requires 20 bars");
      double sum=0;
      for(size_t i=v.size()-20;i<v.size();++i){
        const auto& e=v[i];const double hl=std::log(e.b/e.c),co=std::log(e.d/e.a);
        sum+=0.5*hl*hl-(2.0*std::numbers::ln2_v<double>-1.0)*co*co;
      }
      return valid(std::sqrt(std::max(0.0,sum/20.0)));
    }});
  // ASEP2 amihud_roll: a 20-return window, requiring positive quote-notional
  // volume in every included bar. This is NOT verified dollar volume on every venue.
  out.push_back({"amihud20","asep2-amihud-native-v1","returns=20;denom=close*base_volume",{primary},{},BAR_TTL,
    [primary](const ReadView& view,const Results&){
      const auto& v=history(view,primary);
      if(v.size()<21)return missing("Amihud requires 21 bars / 20 returns");
      double total=0;
      for(size_t i=v.size()-20;i<v.size();++i){
        if(v[i].e<=0)return missing("Amihud undefined for zero traded volume");
        const double r=std::log(v[i].d/v[i-1].d);
        total+=std::abs(r)/(v[i].d*v[i].e);
      }
      return valid(total/20.0);
    }});
  // ASEP2 volume engine: volume-surge-weighted OBV slope. Adaptation uses a
  // local 30-bar OBV origin and normalizes by mean volume, so it is independent
  // of arbitrary earlier history and units. It is NOT a byte-for-byte score port.
  out.push_back({"volume_obv30","asep2-obv-surge-adapted-v1","obv_window=30;surge_window=20",{primary},{},BAR_TTL,
    [primary](const ReadView& view,const Results&){
      const auto& v=history(view,primary);
      if(v.size()<30)return missing("OBV/volume requires 30 bars");
      double obv[30]{};double sum30=0,sum20=0;
      for(size_t j=0;j<30;++j){
        const size_t i=v.size()-30+j;
        if(j){const double change=v[i].d-v[i-1].d;obv[j]=obv[j-1]+(change>0?v[i].e:change<0?-v[i].e:0.0);}
        sum30+=v[i].e;if(j>=10)sum20+=v[i].e;
      }
      const double avg30=sum30/30.0,avg20=sum20/20.0;
      if(avg30<=0||avg20<=0)return missing("OBV surge undefined for zero average volume");
      constexpr double xbar=14.5;
      double ybar=0;for(double z:obv)ybar+=z;ybar/=30.0;
      double num=0,den=0;
      for(size_t j=0;j<30;++j){const double dx=static_cast<double>(j)-xbar;num+=dx*(obv[j]-ybar);den+=dx*dx;}
      const double normalized_slope=(num/den)/avg30;
      const double surge=v.back().e/avg20;
      return valid(0.5*std::tanh(normalized_slope)*std::clamp(surge,0.0,2.0));
    }});
  // ASEP2 changepoint: only its CUSUM component, NOT the separate BOCPD model.
  // Population-standardize the last 40 log returns; output final max CUSUM.
  out.push_back({"cusum40","asep2-cusum-only-native-v1","returns=40;k=0.5;threshold=5",{primary},{},BAR_TTL,
    [primary](const ReadView& view,const Results&){
      const auto& v=history(view,primary);
      if(v.size()<41)return missing("CUSUM requires 41 bars / 40 returns");
      double returns[40];double mean=0;
      for(size_t j=0;j<40;++j){const size_t i=v.size()-40+j;returns[j]=std::log(v[i].d/v[i-1].d);mean+=returns[j];}
      mean/=40.0;
      double variance=0;for(double x:returns){const double z=x-mean;variance+=z*z;}variance/=40.0;
      const double sd=std::sqrt(variance);
      if(sd<=1e-14)return valid(0.0); // flat history: no change detection evidence
      double positive=0,negative=0;
      for(size_t j=1;j<40;++j){
        const double z=(returns[j]-mean)/sd;
        positive=std::max(0.0,positive+z-0.5);
        negative=std::max(0.0,negative-z-0.5);
      }
      return valid(std::max(positive,negative));
    }});
  return out;
}
}
