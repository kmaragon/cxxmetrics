#pragma once

namespace cxxmetrics {

/**
 * \brief A composite metric
 *
 * This enapsulates things like a timer which are a composite of several
 * sources: An ewma, a distribution, a counter.
 *
 * The path to a metric source from a registry is through the name, the tags,
 * and then the composite key.
 */
class composite_metric {};

} // namespace cxxmetrics