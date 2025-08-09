#pragma once

namespace cxxmetrics::metrics {

class metric_pointer;

/**
 * \brief The base class for a container that holds metrics
 */
class basic_metric_container {
public:
  virtual ~basic_metric_container() = default;

  bool insert(const metric_pointer& m);

  void remove(const metric_pointer& m);

protected:

  /**
   * \brief Add a reference to some metric m in the container
   *
   * \note the rules around invalidate below. If an item has been invalidated it can still be in the collection but a new conflicting item must be allowed
   *
   * This should also never be called directly. Inserting must always be done via insert() above
   *
   * \return true if the item was added, false if it (or a conflicting item) already existed in the container
   */
  virtual bool add(const metric_pointer& m) = 0;

  /**
   * \brief Remove a reference to some metric m in the container
   *
   * This is called invalidate because it should be set up such that an attempt
   * to re-add another metric with some conflicting key be allowed to succeed
   * rather than rewnewing this one. And there's an inherent race where that
   * can happen.
   */
  virtual void invalidate(const metric_pointer& m) = 0;
};

} // namespace cxxmetrics::metrics