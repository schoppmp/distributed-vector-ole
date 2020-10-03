#include "distributed_vector_ole.h"

namespace distributed_vector_ole {

const std::vector<int> VOLEParameters::output_size = {4096, 16384, 65536, 616092, 10616092};
const std::vector<int> VOLEParameters::seed_size = {1589, 3482, 7391, 37248, 588160};
const std::vector<int> VOLEParameters::num_noise_indices = {98, 198, 382, 1254, 1324};
const int VOLEParameters::kCodeGeneratorNonzeros = 10;

}  // namespace distributed_vector_ole