#ifndef CLOCK_HXX_
#define CLOCK_HXX_

#include "../gl_compat.hxx"

class Clock
{
  public:

    /* Constructor */
    explicit Clock();

    /* Restart clock */
    void restart();

    /* Get elapsed time */
    double getElapsedTime();

  private:

    /* Time value */
    double m_time;
};

#endif
