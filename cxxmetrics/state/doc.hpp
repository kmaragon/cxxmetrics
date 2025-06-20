/**
 * \namespace cxxmetrics::state
 *
 * The state is where the actual metrics engine lives. User code has a handle to
 * the types here. Through that handle, they record metrics. The registry has
 * a pointer to the types here. Through that handle, they can publish the
 * metrics to a metric sink.
 *
 * As such, other parts of the library are only interested in
 * calling into the state to record metrics and to publish the current values
 * as needed. But the actual data structures that handle recording metrics and
 * snapshotting them are in this namespace.
 */