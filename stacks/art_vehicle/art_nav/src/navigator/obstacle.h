/* -*- mode: C++ -*-
 *
 *  Navigator obstacle class
 *
 *  Copyright (C) 2007, 2010, Austin Robot Technology
 *  License: Modified BSD Software License Agreement
 *
 *  $Id$
 */

#ifndef _OBSTACLE_HH_
#define _OBSTACLE_HH_

#include <vector>
#include "ntimer.h"

#include <art_msgs/Observers.h>

// Provide short names for these messages so they can easily be moved
// to a different package.
using art_msgs::Observers;
using art_msgs::Observation;

/** @brief Navigator obstacle class.
 *
 *  @todo Add ROS-style ObstacleGrid input.
 *  @todo Add ART-style Observers input.
 */
class Obstacle
{
 public:

  /** @brief Constructor */
  Obstacle(Navigator *_nav, int _verbose);

  /** @brief Destructor */
  ~Obstacle()
  {
    delete blockage_timer;
  };

  /** @brief update blockage_timer (do once per run cycle)
   *  
   *  @returns true if timeout reached
   */
  bool blockage_timeout(void)
  {
    return blockage_timer->Check();
  }

  /** @brief mark car blocked, (re)start timeout. */
  void blocked(void)
  {
    if (verbose >= 5)
      ART_MSG(3, "Starting blockage timer");
    blockage_timer->Start(blockage_timeout_secs);
  }

  /** @brief is there a car approaching from ahead in our lane? */
  bool car_approaching();

  /** @brief set configuration variables. */
  void configure();

  /** @brief return distance to closest obstacle ahead in course plan */
  float closest_ahead_in_plan(void);

  /** Return distances of closest obstacles ahead and behind in a lane. */
  void closest_in_lane(const poly_list_t &lane, float &ahead, float &behind);

  /** @brief return true if class initialized */
  bool initialized(void)
  {
    //return lasers->have_ranges;
    return true;
  };

  /** @brief maximum scan range accessor. */
  float maximum_range(void) {return max_range;}

  /** @brief return current observation state */
  Observation observation(Observation::_oid_type oid)
  {
    return obstate.obs[oid];
  }

  /** @brief return true when observer reports clear to go */
  bool observer_clear(Observation::_oid_type oid)
  {
    bool clear = obstate.obs[oid].clear && obstate.obs[oid].applicable;

    // if waiting on observers, reset the blockage_timer
    if (!clear)
      blocked();
    return clear;
  }

  /** @brief handle observers driver message
   *
   *  Called from the driver ProcessMessage() handler when new
   *  observers data arrive.
   *
   * @param hdr the player message header pointer
   * @param obs_msg pointer to the observer state message struct in
   * the player message queue.  Must copy the data before returning.
   */
  void observers_message(Observers *obs_msg);

  /** @brief return true when observer reports passing lane clear */
  bool passing_lane_clear(void);

  /** @brief reset obstacles. */
  void reset(void);

  /** @brief mark car not blocked, cancel timeout. */
  void unblocked(void)
  {
    if (verbose >= 5)
      ART_MSG(3, "Canceling blockage timer");
    blockage_timer->Cancel();
  }
  
  /** @brief update blockage timer state. */
  void update_blockage_state(void)
  {
    if (navdata->stopped != was_stopped ||
	!navdata->stopped)
      {
	if (navdata->stopped)
	  blocked();
	else
	  unblocked();
	if (verbose >= 4 &&
	    navdata->stopped != was_stopped)
	  ART_MSG(8, "vehicle %s moving",
		  (navdata->stopped? "stopped": "started"));
	was_stopped = navdata->stopped;
      }
  }

 private:

  // parameters
  float max_range;			//< maximum scan range

  // observers data
  Observers obstate;                    //< current observers state
  Observers prev_obstate;               //< previous observers state

  // blockage timer
  NavTimer *blockage_timer;
  bool was_stopped;			// previous cycle's stop state

  // .cfg variables
  double blockage_timeout_secs;
  double lane_width_ratio;
  double lane_scan_angle;
  double max_obstacle_dist;
  double min_approach_speed;
  bool   offensive_driving;

  // constructor parameters
  int verbose;				// message verbosity level
  Navigator *nav;			// internal navigator class

  // convenience pointers to Navigator class data
  PolyOps* pops;			// polygon operations class
  Course* course;			// course planning class
  art_msgs::Order *order;               // current commander order
  art_msgs::NavigatorState *navdata;    // current navigator state
  nav_msgs::Odometry *odom;             // current odometry position
  nav_msgs::Odometry *estimate;         // estimated control position
  Config *config_;                      // current configuration

  // returns true if obstacle is within the specified lane
  bool in_lane(MapXY location, const poly_list_t &lane, int start_index);

#if 0 // ignoring obstacles at the moment
  // conversions between laser scan indices and bearings
  float laser_scan_bearing(unsigned index, const player_laser_data_t &scan)
  {
    return scan.min_angle + index * scan.resolution;
  }
  unsigned laser_scan_index(float bearing, const player_laser_data_t &scan)
  {
    unsigned index =
      (unsigned) rintf((bearing - scan.min_angle) / scan.resolution);
    if (index < 0)
      index = 0;
    else if (index > scan.ranges_count)
      index = scan.ranges_count;
    return index;
  }
#endif // ignoring obstacles at the moment
};

#endif // _OBSTACLE_HH_
