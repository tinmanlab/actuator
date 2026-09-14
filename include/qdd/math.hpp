#pragma once
#include <algorithm>
#include <cmath>
namespace qdd {
constexpr float pi = 3.14159265358979323846f;
constexpr float sqrt3 = 1.7320508075688772935f;
template<class T> struct Phase { T a{}, b{}, c{}; };
template<class T> struct AB { T alpha{}, beta{}; };
template<class T> struct Dq { T d{}, q{}; };
using ABC=Phase<float>; using AlphaBeta=AB<float>; using DQ=Dq<float>;
template<class T> inline T limit(T x,T lo,T hi) { return std::max(lo,std::min(x,hi)); }
template<class T> inline AB<T> clarke(Phase<T> x) {
 // Full three-input amplitude-invariant transform. Zero sequence is rejected.
 return {(T(2)*x.a-x.b-x.c)/T(3),(x.b-x.c)/std::sqrt(T(3))};
}
template<class T> inline Phase<T> inverse_clarke(AB<T> x) {
 return {x.alpha, -x.alpha/T(2)+std::sqrt(T(3))*x.beta/T(2),
                    -x.alpha/T(2)-std::sqrt(T(3))*x.beta/T(2)};
}
template<class T> inline Dq<T> park(AB<T> x,T angle) {
 const T c=std::cos(angle),s=std::sin(angle);
 return {c*x.alpha+s*x.beta,-s*x.alpha+c*x.beta};
}
template<class T> inline AB<T> inverse_park(Dq<T> x,T angle) {
 const T c=std::cos(angle),s=std::sin(angle);
 return {c*x.d-s*x.q,s*x.d+c*x.q};
}
template<class T> inline T magnitude(Dq<T> x) {return std::hypot(x.d,x.q); }
template<class T> inline Dq<T> circle_limit(Dq<T> x,T radius) {
 const T m=magnitude(x); if(m>radius && m>T(0)) { x.d*=radius/m; x.q*=radius/m; } return x;
}
template<class T> inline T phase_power(Phase<T> v,Phase<T> i) {return v.a*i.a+v.b*i.b+v.c*i.c;}
template<class T> inline T dq_power(Dq<T> v,Dq<T> i) {return T(1.5)*(v.d*i.d+v.q*i.q);}
template<class T> inline T peak_abs(Phase<T> v) {return std::max({std::abs(v.a),std::abs(v.b),std::abs(v.c)});}
template<class T> inline bool finite(Phase<T> v) {return std::isfinite(v.a)&&std::isfinite(v.b)&&std::isfinite(v.c);}
}
