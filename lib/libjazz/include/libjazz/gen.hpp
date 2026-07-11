#ifndef GEN_H_
#define GEN_H_

#include <rapidcheck.h>

namespace jazz {
namespace gen {

/** Generate an arbitrary float in interval [0.0, 1.0] */
rc::Gen<float> ratio();

/** Generate an arbitrary float in interval [-1.0, 1.0] */
rc::Gen<float> signedRatio();

}  // namespace gen
}  // namespace jazz

#endif  // GEN_H_
