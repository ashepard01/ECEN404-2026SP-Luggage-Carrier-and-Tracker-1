#ifndef EVENTS_H
#define EVENTS_H

typedef enum {
    EVENT_WAYPOINT_RECEIVED,
    EVENT_WAYPOINT_REACHED,
    EVENT_OBSTACLE_DETECTED,
    EVENT_OBSTACLE_CLEARED,
    EVENT_EMERGENCY_STOP,
    EVENT_EMERGENCY_CLEARED,
    EVENT_ERROR,
    EVENT_WEIGHT_REMOVED,
    EVENT_WEIGHT_RESTORED
} Event_t;

typedef enum {
    STATE_IDLE,              // Stopped, waiting
    STATE_NAVIGATING,        // Following waypoint // Avoiding obstacle
    STATE_EMERGENCY_STOP,     // Emergency stop active
    STATE_ERROR,              // Error/fault state
    STATE_ALARM,
    STATE_MANUAL_DRIVE
} State_t;


#endif




