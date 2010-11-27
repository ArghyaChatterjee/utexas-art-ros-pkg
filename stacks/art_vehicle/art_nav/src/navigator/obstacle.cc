/*
 *  Navigator obstacle class
 *
 *  Copyright (C) 2007, 2010, Austin Robot Technology
 *  License: Modified BSD Software License Agreement
 *
 *  $Id$
 */

//#include <art/DARPA_rules.h>
#include <art/infinity.h>
//#include <art/Safety.h>

#include "navigator_internal.h"
#include "course.h"
#include "obstacle.h"

// Constructor
Obstacle::Obstacle(Navigator *_nav, int _verbose)
{
  verbose = _verbose;
  nav = _nav;

  // copy convenience pointers to Navigator class data
  course = nav->course;
  estimate = &nav->estimate;
  navdata = &nav->navdata;
  odom = nav->odometry;
  order = &nav->order;
  pops = nav->pops;
  config_ = &nav->config_;

  // TODO Make this a parameter
  max_range = 80.0;

  // initialize observers state to all clear in case that driver is
  // not subscribed or not publishing data
  obstate.obs.resize(Observers::N_Observers);
  prev_obstate.obs.resize(Observers::N_Observers);
  for (unsigned i = 0; i < Observers::N_Observers; ++i)
    {
      obstate.obs[i].clear = true;
      obstate.obs[i].applicable = true;
    }

  // allocate timers
  blockage_timer = new NavTimer();
  was_stopped = false;

  reset();
}

// is there a car approaching from ahead in our lane?
//
//  Note: must *not* reset blockage_timer.
//
bool Obstacle::car_approaching()
{
  if (offensive_driving)
    return false;

  Observation::_oid_type oid = Observers::Nearest_forward;
  if (obstate.obs[oid].clear || !obstate.obs[oid].applicable)
    {
      if (verbose >= 4)
	ART_MSG(6, "no known car approaching from ahead");
      return false;			// clear ahead
    }

  // estimate absolute speed of obstacle from closing velocity and
  // vehicle speed at time of observation
  float rel_speed = obstate.obs[oid].velocity;
  float abs_speed = rel_speed - obstate.odom.twist.twist.linear.x;

  if (verbose >= 3)
    ART_MSG(6, "obstacle observed closing at %.3fm/s, absolute speed %.3f",
	    rel_speed, abs_speed);

  return (abs_speed > min_approach_speed);
}

// Return distance to closest obstacle ahead in course plan.
//
// entry:
//	course->plan contains current lane polygons
//
float Obstacle::closest_ahead_in_plan(void)
{
  // value to return if no obstacle found
  float retval = Infinite::distance;

#if 0 // ignoring obstacles at the moment
  // find index of closest polygon to current vehicle pose
  int closest_poly_index = pops->getClosestPoly(course->plan, estimate->pos);
  if (closest_poly_index < 0)
    {
      if (verbose)
	ART_MSG(2, "no closest polygon in lane (size %u)",
		course->plan.size());
      return retval;
    }

  // When rejoining a lane, there should be an aim_poly defined.  Only
  // look for obstacles starting there.
  if (course->aim_poly.poly_id != -1)
    {
      int aim_index = pops->getPolyIndex(course->plan, course->aim_poly);
      if (aim_index >=0 && aim_index < (int) course->plan.size())
	closest_poly_index = aim_index;
    }

  for (uint i=0; i< lasers->all_obstacle_list.size(); i++)
    if (in_lane(lasers->all_obstacle_list.at(i), 
		course->plan, closest_poly_index))
      {
	if (verbose >4)
	  ART_MSG(3,"Saw obstacle in lane: obstacle list size %d (velodyne %d, fusion %d) at index %d",lasers->all_obstacle_list.size(), lasers->velodyne_obstacle_list.size(),lasers->fusion_obstacle_list.size(),  i);
	
	float distance = course->distance_in_plan(estimate->pos,
						  lasers->all_obstacle_list.at(i));						

	if (distance > 0 && distance < retval)
	  {
	    retval = distance;
	  }
      }

  if (verbose >= 3)
    ART_MSG(8, "Closest obstacle in lane %s is %.3f m down the lane",
	    course->plan.at(closest_poly_index).start_way.lane_name().str,
	    retval);
#endif // ignoring obstacles

  return retval;
}

// Return distances of closest obstacles ahead and behind in a lane.
//
// entry:
//	lane = list of polygons to check
// exit:
//	ahead = distance along lane to closest obstacle ahead
//	behind = distance along lane to closest obstacle behind
//	(returns Infinite::distance if none in range)
//
void Obstacle::closest_in_lane(const poly_list_t &lane,
			       float &ahead, float &behind)
{
  // values to return if no obstacles found
  ahead = Infinite::distance;
  behind = Infinite::distance;

  int closest_poly_index =
    pops->getClosestPoly(lane, MapPose(estimate->pose.pose));
  if (lane.empty() || closest_poly_index < 0)
    {
      ROS_DEBUG_STREAM("no closest polygon in lane (size "
                       << lane.size() << ")");
      return;
    }

#if 0 // ignoring obstacles at the moment
  
  for (uint i=0; i< lasers->all_obstacle_list.size(); i++)
    if (in_lane(lasers->all_obstacle_list.at(i), lane, 0))
      {
	float distance = pops->distanceAlongLane(lane, estimate->pos,
						 lasers->all_obstacle_list.at(i));					
	if (distance < 0)
	  behind = fminf(behind, -distance);
	else
	  ahead = fminf(ahead, distance);
      }

#endif // ignoring obstacles at the moment

  ROS_DEBUG("Closest obstacles in lane %s are %.3fm ahead and %.3fm behind",
	    lane.at(closest_poly_index).start_way.lane_name().str,
	    ahead, behind);
}

void Obstacle::configure()
{
  blockage_timeout_secs = config_->blockage_timeout_secs;
  lane_width_ratio = config_->lane_width_ratio;
  lane_scan_angle = config_->lane_scan_angle;
  max_obstacle_dist = config_->max_obstacle_dist;
  min_approach_speed = config_->min_approach_speed;
  offensive_driving = config_->offensive_driving;

#if 0
  ros::NodeHandle nh("~");

  nh.param( "blockage_timeout_secs", blockage_timeout_secs, 9.0);
  ROS_INFO("\tblockage timeout is %.1f", blockage_timeout_secs);

  // lane width ratio (in range of 0.01 to 1.0)
  nh.param( "lane_width_ratio", lane_width_ratio, 0.3);
  lane_width_ratio = fmaxf(lane_width_ratio, 0.01f);
  lane_width_ratio = fminf(lane_width_ratio, 1.0f);
  ROS_INFO("\tlane width ratio is %.3f", lane_width_ratio);

  // lane scan angle (in range of 0 to pi)
  nh.param("lane_scan_angle", lane_scan_angle, M_PI/3.0);
  lane_scan_angle = fmaxf(lane_scan_angle, 0.0f);
  lane_scan_angle = fminf(lane_scan_angle, (float) M_PI);
  ROS_INFO("\tlane scan angle is %.3f radians", lane_scan_angle);

  // distance at which we start paying attention to an obstacle
  nh.param("max_obstacle_dist", max_obstacle_dist, 100.0);
  ROS_INFO("\tmaximum obstacle distance considered is %.3f meters",
	  max_obstacle_dist);

  // minimum approach speed considered dangerous
  nh.param("min_approach_speed", min_approach_speed, 2.0);
  ROS_INFO("\tminimum approach speed is %.3f m/s", min_approach_speed);

  nh.param("offensive_driving", offensive_driving, false); 
  ROS_INFO("\tuse %s driving style",
	  (offensive_driving? "offensive": "defensive"));
#endif
}

// returns true if obstacle is within the specified lane
bool Obstacle::in_lane(MapXY location, const poly_list_t &lane,
		       int start_index)
{
  start_index=std::max(0,start_index);
  start_index=std::min(start_index, int(lane.size()-1));

  poly in_poly= lane.at(start_index);

  // figure out what polygon contains the obstacle
  for (unsigned j = start_index; j < lane.size(); j++)
    {
      poly curPoly = lane[j];
      if (pops->pointInPoly_ratio(location, curPoly, lane_width_ratio))
	{
	  // this obstacle is within that lane
	  if (verbose >= 5)
	    {
	      ART_MSG(8, "obstacle at (%.3f, %.3f) is in lane %s, polygon %d",
		      location.x, location.y, 
		      in_poly.start_way.lane_name().str, curPoly.poly_id);
	    }
	  return true;			// terminate polygon search
	}
    }
  
  if (verbose >= 6)
    ART_MSG(8, "obstacle at (%.3f, %.3f) is outside our lane",
	    location.x, location.y);

  return false;
}

// handle observers driver message
//
//  Called from the driver ProcessMessage() handler when new
//  observers data arrive.
//
void Obstacle::observers_message(Observers *obs_msg)
{
  if (obs_msg->obs.size() != Observers::N_Observers)
    {
      // error in message size
      ROS_ERROR_STREAM("ERROR: Observer message size (" << obs_msg->obs.size()
                       << ") is wrong (ignored)");
      return;
    }

  if (obstate.header.stamp == obs_msg->header.stamp)
    return;				// repeated message, no new data

  prev_obstate = obstate;
  obstate = *obs_msg;

  if (verbose >= 2)
    {
      char clear_string[Observers::N_Observers+1];
      for (unsigned i = 0; i < Observers::N_Observers; ++i)
	{
	  clear_string[i] = (obstate.obs[i].clear? '1': '0');
	  if (obstate.obs[i].applicable)
	    clear_string[i] += 2;
	}
      clear_string[Observers::N_Observers] = '\0';
      ROS_DEBUG("observers report {%s}, pose (%.3f,%.3f,%.3f), "
                "(%.3f, %.3f), time %.6f",
                clear_string,
                obstate.odom.pose.pose.position.x,
                obstate.odom.pose.pose.position.y,
                tf::getYaw(obstate.odom.pose.pose.orientation),
                obstate.odom.twist.twist.linear.x,
                obstate.odom.twist.twist.angular.z,
                obstate.header.stamp.toSec());
    }
}

// return true when observer reports passing lane clear
bool Obstacle::passing_lane_clear(void)
{
  if (course->passing_left)
    return observer_clear(Observers::Adjacent_left);
  else
    return observer_clear(Observers::Adjacent_right);
}

// reset obstacle class
void Obstacle::reset(void)
{
  unblocked();
}
