/**
 * \namespace cxxmetrics::metrics
 *
 * The types in this library wrap a handle to the state to allow user code to
 * record metrics through a handle that manages the lifetime of the metric
 * registration. If the handles are sharing state with a registry, they also
 * keep the metric registered in said registry. When the last handle is released,
 * the metric is removed from the registry.
 */