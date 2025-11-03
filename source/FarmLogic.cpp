#include "FarmLogic.h"
#include "displayobject.hpp"
#include <unistd.h>
#include <thread>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <map>
#include <set>
#include <queue>
#include <algorithm>
#include <iostream>
#include <cmath>

// Display synchronization
std::mutex display_mtx;

// Game state synchronization
std::mutex position_mtx;  // For tracking entity positions
std::mutex nest_mtx;       // For nest operations
std::mutex barn_mtx;       // For barn operations  
std::mutex bakery_mtx;     // For bakery operations
std::mutex shop_mtx;       // For shop operations
std::mutex intersection_mtx; // For truck intersection
std::mutex stats_mtx;      // For stats updates

// Condition variables for waiting
std::condition_variable nest_cv;
std::condition_variable barn_cv;
std::condition_variable bakery_cv;
std::condition_variable oven_cv;
std::condition_variable shop_cv;
std::condition_variable intersection_cv;

// Object dimensions
int egg_w = 20;
int egg_h = 20;
int cow_w = 80;
int cow_h = 80;
int truck_w = 90;
int truck_h = 70;
int person_w = 50;
int person_h = 90;
int chicken_w = 45;
int chicken_h = 45;

// Key locations on the farm
const int NEST1_X = 100, NEST1_Y = 500;
const int NEST2_X = 700, NEST2_Y = 500;
const int BARN1_X = 50, BARN1_Y = 50;   // Butter/eggs barn
const int BARN2_X = 50, BARN2_Y = 150;  // Flour/sugar barn
const int BAKERY_X = 550, BAKERY_Y = 150;
const int INTERSECTION_X = 300, INTERSECTION_Y = 150;
const int SHOP_X = 650, SHOP_Y = 150;

// Game state tracking
struct Position {
    int x, y;
    int width, height;
    int layer;
};

std::map<int, Position> entity_positions;

struct NestState {
    int egg_count = 0;
    std::vector<int> eggs_by_chicken;
    bool occupied = false;
    int occupant_id = -1;
};

std::map<int, NestState> nest_states;

struct BarnState {
    int eggs = 0;
    int butter = 0;
    int flour = 0;
    int sugar = 0;
};

BarnState barn1_state;
BarnState barn2_state;

struct BakeryState {
    int eggs = 0;
    int butter = 0;
    int flour = 0;
    int sugar = 0;
    int cakes = 0;
    bool oven_busy = false;
};

BakeryState bakery_state;

// Shop state
std::queue<int> child_queue;
int current_shopper = -1;
int cakes_needed = 0;

// Intersection state
bool intersection_occupied = false;
std::queue<int> truck_queue;

// Display object pointers for egg management
std::map<int, std::vector<DisplayObject*>> nest_eggs;

// Global stats tracking
BakeryStats global_stats;

// Helper functions
bool out_of_bounds(DisplayObject &obj, int x, int y) {
    int left = (obj.x+x)-(obj.width/2);
    int right = (obj.x+x)+(obj.width/2);
    int up = (obj.y+y)+(obj.height/2);
    int down = (obj.y+y)-(obj.height/2);
    
    return (left<0 || right>800 || up>600 || down<0);
}

bool check_collision(int x1, int y1, int w1, int h1, 
                    int x2, int y2, int w2, int h2) {
    int left1 = x1 - w1/2, right1 = x1 + w1/2;
    int left2 = x2 - w2/2, right2 = x2 + w2/2;
    int top1 = y1 + h1/2, bottom1 = y1 - h1/2;
    int top2 = y2 + h2/2, bottom2 = y2 - h2/2;
    
    return !(left1 > right2 || right1 < left2 || 
             top1 < bottom2 || bottom1 > top2);
}

bool can_move_to(int id, int new_x, int new_y, int width, int height, int layer) {
    std::lock_guard<std::mutex> lk(position_mtx);
    
    for (auto& [other_id, pos] : entity_positions) {
        if (other_id != id && pos.layer == layer) {
            if (check_collision(new_x, new_y, width, height,
                              pos.x, pos.y, pos.width, pos.height)) {
                return false;
            }
        }
    }
    return true;
}

void update_position(int id, int x, int y, int width, int height, int layer) {
    std::lock_guard<std::mutex> lk(position_mtx);
    entity_positions[id] = {x, y, width, height, layer};
}

// Path helper - move towards target with collision avoidance
bool move_towards(DisplayObject &obj, int id, int target_x, int target_y, 
                  int speed, int width, int height, int layer) {
    int dx = 0, dy = 0;
    int dist_x = target_x - obj.x;
    int dist_y = target_y - obj.y;
    
    // Calculate movement direction
    if (abs(dist_x) > speed) {
        dx = (dist_x > 0) ? speed : -speed;
    } else {
        dx = dist_x;
    }
    
    if (abs(dist_y) > speed) {
        dy = (dist_y > 0) ? speed : -speed;
    } else {
        dy = dist_y;
    }
    
    // Try primary movement
    if (!out_of_bounds(obj, dx, dy)) {
        int new_x = obj.x + dx;
        int new_y = obj.y + dy;
        
        if (can_move_to(id, new_x, new_y, width, height, layer)) {
            obj.setPos(new_x, new_y);
            update_position(id, new_x, new_y, width, height, layer);
            return true;
        }
        
        // If blocked, try moving in just one direction
        if (dx != 0 && dy != 0) {
            // Try horizontal only
            if (can_move_to(id, obj.x + dx, obj.y, width, height, layer)) {
                obj.setPos(obj.x + dx, obj.y);
                update_position(id, obj.x + dx, obj.y, width, height, layer);
                return true;
            }
            // Try vertical only
            if (can_move_to(id, obj.x, obj.y + dy, width, height, layer)) {
                obj.setPos(obj.x, obj.y + dy);
                update_position(id, obj.x, obj.y + dy, width, height, layer);
                return true;
            }
        }
        
        // If still blocked, try perpendicular movement to go around
        if (dx == 0) {
            // Moving vertically, try horizontal dodge
            for (int dodge : {speed, -speed}) {
                if (!out_of_bounds(obj, dodge, dy) && 
                    can_move_to(id, obj.x + dodge, obj.y + dy, width, height, layer)) {
                    obj.setPos(obj.x + dodge, obj.y + dy);
                    update_position(id, obj.x + dodge, obj.y + dy, width, height, layer);
                    return true;
                }
            }
        } else if (dy == 0) {
            // Moving horizontally, try vertical dodge
            for (int dodge : {speed, -speed}) {
                if (!out_of_bounds(obj, dx, dodge) && 
                    can_move_to(id, obj.x + dx, obj.y + dodge, width, height, layer)) {
                    obj.setPos(obj.x + dx, obj.y + dodge);
                    update_position(id, obj.x + dx, obj.y + dodge, width, height, layer);
                    return true;
                }
            }
        }
    }
    return false;
}

void display(BakeryStats& stats) {
    while(true) {
        {
            std::lock_guard<std::mutex> lk(display_mtx);
            std::lock_guard<std::mutex> stats_lk(stats_mtx);
            DisplayObject::redisplay(global_stats);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Chicken - moves between nests on a defined path
void chicken(int init_x, int init_y, int id) {
    DisplayObject chicken("chicken", chicken_w, chicken_h, 2, id);
    chicken.setPos(init_x, init_y);
    update_position(id, init_x, init_y, chicken_w, chicken_h, 2);
    
    {
        std::lock_guard<std::mutex> lk(display_mtx);
        chicken.updateFarm();
    }
    
    std::vector<int> nest_ids = {1000, 1001};
    int current_nest_idx = id % 2;
    int eggs_laid_at_current = 0;
    
    while(true) {
        int target_nest_id = nest_ids[current_nest_idx];
        int nest_x = (target_nest_id == 1000) ? NEST1_X : NEST2_X;
        int nest_y = (target_nest_id == 1000) ? NEST1_Y : NEST2_Y;
        
        // Move toward nest
        int attempts = 0;
        while ((abs(chicken.x - nest_x) > 40 || abs(chicken.y - nest_y) > 40) && attempts < 200) {
            move_towards(chicken, id, nest_x, nest_y, 4, chicken_w, chicken_h, 2);
            {
                std::lock_guard<std::mutex> disp_lk(display_mtx);
                chicken.updateFarm();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            attempts++;
        }
        
        // At nest - try to lay eggs with timeout
        bool laid_egg = false;
        bool nest_was_full = false;
        {
            std::unique_lock<std::mutex> nest_lk(nest_mtx);
            
            // Wait with timeout to avoid indefinite blocking
            auto result = nest_cv.wait_for(nest_lk, std::chrono::milliseconds(1000), [&] {
                return !nest_states[target_nest_id].occupied || 
                       nest_states[target_nest_id].occupant_id == id;
            });
            
            if (result && nest_states[target_nest_id].egg_count < 3) {
                nest_states[target_nest_id].occupied = true;
                nest_states[target_nest_id].occupant_id = id;
                
                int egg_index = nest_states[target_nest_id].egg_count;
                nest_states[target_nest_id].egg_count++;
                nest_states[target_nest_id].eggs_by_chicken.push_back(id);
                
                {
                    std::lock_guard<std::mutex> stats_lk(stats_mtx);
                    global_stats.eggs_laid++;
                }
                
                if (egg_index < nest_eggs[target_nest_id].size()) {
                    int egg_x = (target_nest_id == 1000) ? 90 + (egg_index * 10) : 690 + (egg_index * 10);
                    int egg_y = 507;
                    nest_eggs[target_nest_id][egg_index]->setPos(egg_x, egg_y);
                    
                    {
                        std::lock_guard<std::mutex> disp_lk(display_mtx);
                        nest_eggs[target_nest_id][egg_index]->updateFarm();
                    }
                }
                
                eggs_laid_at_current++;
                laid_egg = true;
                
                nest_states[target_nest_id].occupied = false;
                nest_states[target_nest_id].occupant_id = -1;
            } else if (result && nest_states[target_nest_id].egg_count >= 3) {
                nest_was_full = true;
            }
            
            nest_cv.notify_all();
        }
        
        // Move away from nest a bit (wander)
        int wander_x = chicken.x + ((std::rand() % 81) - 40);
        int wander_y = chicken.y + ((std::rand() % 81) - 40);
        
        // Keep within bounds
        wander_x = std::max(50, std::min(750, wander_x));
        wander_y = std::max(250, std::min(550, wander_y));
        
        attempts = 0;
        while ((abs(chicken.x - wander_x) > 10 || abs(chicken.y - wander_y) > 10) && attempts < 50) {
            move_towards(chicken, id, wander_x, wander_y, 4, chicken_w, chicken_h, 2);
            {
                std::lock_guard<std::mutex> disp_lk(display_mtx);
                chicken.updateFarm();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            attempts++;
        }
        
        // Switch nests after laying 3 eggs or if nest is full
        if (eggs_laid_at_current >= 3 || nest_was_full) {
            current_nest_idx = (current_nest_idx + 1) % nest_ids.size();
            eggs_laid_at_current = 0;
        }
        
        // Wait a bit before next action
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}

// Farmer - follows path between barn and nests
void farmer(int init_x, int init_y, int id) {
    DisplayObject farmer("farmer", person_w, person_h, 2, id);
    farmer.setPos(init_x, init_y);
    update_position(id, init_x, init_y, person_w, person_h, 2);
    
    {
        std::lock_guard<std::mutex> lk(display_mtx);
        farmer.updateFarm();
    }
    
    std::vector<int> nest_ids = {1000, 1001};
    int current_nest = 0;
    
    while(true) {
        int target_nest_id = nest_ids[current_nest];
        int nest_x = (target_nest_id == 1000) ? NEST1_X : NEST2_X;
        int nest_y = (target_nest_id == 1000) ? NEST1_Y : NEST2_Y;
        
        // Move toward nest - don't require exact paths
        int attempts = 0;
        while ((abs(farmer.x - nest_x) > 30 || abs(farmer.y - nest_y) > 30) && attempts < 300) {
            move_towards(farmer, id, nest_x, nest_y, 5, person_w, person_h, 2);
            {
                std::lock_guard<std::mutex> disp_lk(display_mtx);
                farmer.updateFarm();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            attempts++;
        }
        
        // Collect eggs at nest
        {
            std::unique_lock<std::mutex> nest_lk(nest_mtx);
            
            if (nest_states[target_nest_id].egg_count > 0 && 
                !nest_states[target_nest_id].occupied) {
                int eggs_collected = nest_states[target_nest_id].egg_count;
                nest_states[target_nest_id].egg_count = 0;
                nest_states[target_nest_id].eggs_by_chicken.clear();
                
                {
                    std::lock_guard<std::mutex> barn_lk(barn_mtx);
                    barn1_state.eggs += eggs_collected;
                }
                
                {
                    std::lock_guard<std::mutex> disp_lk(display_mtx);
                    for (int i = 0; i < eggs_collected; i++) {
                        if (i < nest_eggs[target_nest_id].size()) {
                            nest_eggs[target_nest_id][i]->setPos(-100, -100);
                            nest_eggs[target_nest_id][i]->updateFarm();
                        }
                    }
                }
                
                barn_cv.notify_all();
            }
        }
        
        // Move back toward barn area (doesn't have to reach exact position)
        attempts = 0;
        while ((abs(farmer.x - BARN1_X) > 50 || abs(farmer.y - BARN1_Y) > 50) && attempts < 200) {
            move_towards(farmer, id, BARN1_X, BARN1_Y, 5, person_w, person_h, 2);
            {
                std::lock_guard<std::mutex> disp_lk(display_mtx);
                farmer.updateFarm();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            attempts++;
        }
        
        // Switch to next nest
        current_nest = (current_nest + 1) % nest_ids.size();
        
        // Wait before next collection round
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

// Truck - follows specific path with intersection synchronization
void truck(int init_x, int init_y, int id, bool is_barn1) {
    DisplayObject truck("truck", truck_w, truck_h, 2, id);
    truck.setPos(init_x, init_y);
    update_position(id, init_x, init_y, truck_w, truck_h, 2);
    
    {
        std::lock_guard<std::mutex> lk(display_mtx);
        truck.updateFarm();
    }
    
    int barn_x = BARN1_X;
    int barn_y = is_barn1 ? BARN1_Y : BARN2_Y;
    
    struct Cargo {
        int eggs = 0;
        int butter = 0;
        int flour = 0;
        int sugar = 0;
    } cargo;
    
    while(true) {
        // Move to barn
        while (abs(truck.x - barn_x) > 10 || abs(truck.y - barn_y) > 10) {
            if (move_towards(truck, id, barn_x, barn_y, 5, truck_w, truck_h, 2)) {
                {
                    std::lock_guard<std::mutex> disp_lk(display_mtx);
                    truck.updateFarm();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Load at barn
        {
            std::unique_lock<std::mutex> barn_lk(barn_mtx);
            
            if (is_barn1) {
                barn_cv.wait(barn_lk, [&] { return barn1_state.eggs >= 3; });
                cargo.eggs = std::min(3, barn1_state.eggs);
                cargo.butter = 3;
                barn1_state.eggs -= cargo.eggs;
                barn1_state.butter += 3;
                
                {
                    std::lock_guard<std::mutex> stats_lk(stats_mtx);
                    global_stats.butter_produced += 3;
                }
            } else {
                cargo.flour = 3;
                cargo.sugar = 3;
                barn2_state.flour += 3;
                barn2_state.sugar += 3;
                
                {
                    std::lock_guard<std::mutex> stats_lk(stats_mtx);
                    global_stats.flour_produced += 3;
                    global_stats.sugar_produced += 3;
                }
            }
        }
        
        // Path to intersection: Move horizontally
        while (abs(truck.x - INTERSECTION_X) > 10) {
            if (move_towards(truck, id, INTERSECTION_X, truck.y, 5, truck_w, truck_h, 2)) {
                {
                    std::lock_guard<std::mutex> disp_lk(display_mtx);
                    truck.updateFarm();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Request intersection access
        {
            std::unique_lock<std::mutex> int_lk(intersection_mtx);
            truck_queue.push(id);
            
            intersection_cv.wait(int_lk, [&] {
                return !intersection_occupied && 
                       !truck_queue.empty() && 
                       truck_queue.front() == id;
            });
            
            intersection_occupied = true;
            truck_queue.pop();
        }
        
        // Move through intersection to bakery
        while (abs(truck.y - BAKERY_Y) > 10) {
            if (move_towards(truck, id, truck.x, BAKERY_Y, 5, truck_w, truck_h, 2)) {
                {
                    std::lock_guard<std::mutex> disp_lk(display_mtx);
                    truck.updateFarm();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        while (abs(truck.x - BAKERY_X) > 10) {
            if (move_towards(truck, id, BAKERY_X, truck.y, 5, truck_w, truck_h, 2)) {
                {
                    std::lock_guard<std::mutex> disp_lk(display_mtx);
                    truck.updateFarm();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Release intersection
        {
            std::lock_guard<std::mutex> int_lk(intersection_mtx);
            intersection_occupied = false;
            intersection_cv.notify_all();
        }
        
        // Unload at bakery
        {
            std::unique_lock<std::mutex> bakery_lk(bakery_mtx);
            
            bakery_cv.wait(bakery_lk, [&] {
                return (bakery_state.eggs + cargo.eggs <= 6) &&
                       (bakery_state.butter + cargo.butter <= 6) &&
                       (bakery_state.flour + cargo.flour <= 6) &&
                       (bakery_state.sugar + cargo.sugar <= 6);
            });
            
            bakery_state.eggs += cargo.eggs;
            bakery_state.butter += cargo.butter;
            bakery_state.flour += cargo.flour;
            bakery_state.sugar += cargo.sugar;
            
            cargo = {};
            oven_cv.notify_all();
        }
        
        // Return path: Back through intersection
        while (abs(truck.x - INTERSECTION_X) > 10) {
            if (move_towards(truck, id, INTERSECTION_X, truck.y, 5, truck_w, truck_h, 2)) {
                {
                    std::lock_guard<std::mutex> disp_lk(display_mtx);
                    truck.updateFarm();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Move back to barn lane
        int return_y = is_barn1 ? BARN1_Y : BARN2_Y;
        while (abs(truck.y - return_y) > 10) {
            if (move_towards(truck, id, truck.x, return_y, 5, truck_w, truck_h, 2)) {
                {
                    std::lock_guard<std::mutex> disp_lk(display_mtx);
                    truck.updateFarm();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Move to barn
        while (abs(truck.x - barn_x) > 10) {
            if (move_towards(truck, id, barn_x, truck.y, 5, truck_w, truck_h, 2)) {
                {
                    std::lock_guard<std::mutex> disp_lk(display_mtx);
                    truck.updateFarm();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

// Oven thread
void oven_thread() {
    while(true) {
        std::unique_lock<std::mutex> bakery_lk(bakery_mtx);
        
        oven_cv.wait(bakery_lk, [&] {
            return !bakery_state.oven_busy &&
                   bakery_state.eggs >= 2 &&
                   bakery_state.butter >= 2 &&
                   bakery_state.flour >= 2 &&
                   bakery_state.sugar >= 2 &&
                   bakery_state.cakes <= 3;
        });
        
        bakery_state.oven_busy = true;
        bakery_state.eggs -= 2;
        bakery_state.butter -= 2;
        bakery_state.flour -= 2;
        bakery_state.sugar -= 2;
        
        {
            std::lock_guard<std::mutex> stats_lk(stats_mtx);
            global_stats.eggs_used += 2;
            global_stats.butter_used += 2;
            global_stats.flour_used += 2;
            global_stats.sugar_used += 2;
        }
        
        bakery_lk.unlock();
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        bakery_lk.lock();
        bakery_state.cakes += 3;
        bakery_state.oven_busy = false;
        
        {
            std::lock_guard<std::mutex> stats_lk(stats_mtx);
            global_stats.cakes_produced += 3;
        }
        
        shop_cv.notify_all();
        bakery_cv.notify_all();
    }
}

// Child - queues vertically and enters shop one by one
void child(int init_x, int init_y, int id) {
    DisplayObject child("child", person_w, person_h, 2, id);
    
    // Line up children vertically going UP from base
    static std::mutex line_position_mtx;
    static int next_in_line = 0;
    int my_queue_position;
    
    {
        std::lock_guard<std::mutex> lk(line_position_mtx);
        my_queue_position = next_in_line++;
    }
    
    int line_x = 775; 
    int base_line_y = 60;  
    
    int line_y = base_line_y + (my_queue_position * 80); 
    
    child.setPos(line_x, line_y);
    update_position(id, line_x, line_y, person_w, person_h, 2);
    
    {
        std::lock_guard<std::mutex> lk(display_mtx);
        child.updateFarm();
    }
    
    while(true) {
        // Wait for turn to shop (front of line is at bottom)
        {
            std::unique_lock<std::mutex> shop_lk(shop_mtx);
            child_queue.push(id);
            
            shop_cv.wait(shop_lk, [&] {
                return current_shopper == -1 && 
                       !child_queue.empty() && 
                       child_queue.front() == id;
            });
            
            child_queue.pop();
            current_shopper = id;
        }
        
        // Move to shop
        while (abs(child.x - SHOP_X) > 20 || abs(child.y - SHOP_Y) > 20) {
            move_towards(child, id, SHOP_X, SHOP_Y, 4, person_w, person_h, 2);
            {
                std::lock_guard<std::mutex> disp_lk(display_mtx);
                child.updateFarm();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Buy cakes
        int want_cakes = (std::rand() % 6) + 1;
        {
            std::unique_lock<std::mutex> bakery_lk(bakery_mtx);
            shop_cv.wait(bakery_lk, [&] {
                return bakery_state.cakes >= want_cakes;
            });
            
            bakery_state.cakes -= want_cakes;
            
            {
                std::lock_guard<std::mutex> stats_lk(stats_mtx);
                global_stats.cakes_sold += want_cakes;
            }
        }
        
        // Leave shop
        {
            std::lock_guard<std::mutex> shop_lk(shop_mtx);
            current_shopper = -1;
            shop_cv.notify_all();
        }
        
        // Move back to end of line (top of the vertical queue)
        {
            std::lock_guard<std::mutex> lk(line_position_mtx);
            // Move to back (which is top position, index 4)
            my_queue_position = 4;
        }
        
        // Calculate new position at back of line (top)
        line_y = base_line_y + (my_queue_position * 50);
        
        // Move to back of line position
        while (abs(child.x - line_x) > 10 || abs(child.y - line_y) > 10) {
            move_towards(child, id, line_x, line_y, 4, person_w, person_h, 2);
            {
                std::lock_guard<std::mutex> disp_lk(display_mtx);
                child.updateFarm();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // "Eat" cake
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Rotate position - everyone moves down one position
        {
            std::lock_guard<std::mutex> lk(line_position_mtx);
            my_queue_position = (my_queue_position - 1 + 5) % 5;  // Move down in line
        }
        
        // Update position in line
        line_y = base_line_y + (my_queue_position * 50);
        while (abs(child.y - line_y) > 10) {
            move_towards(child, id, child.x, line_y, 4, person_w, person_h, 2);
            {
                std::lock_guard<std::mutex> disp_lk(display_mtx);
                child.updateFarm();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void cow(int init_x, int init_y, int id) {
    DisplayObject cow("cow", cow_w, cow_h, 2, id);
    cow.setPos(init_x, init_y);
    update_position(id, init_x, init_y, cow_w, cow_h, 2);
    
    {
        std::lock_guard<std::mutex> lk(display_mtx);
        cow.updateFarm();
    }
    
    //decided on static cows </3
    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

void FarmLogic::run() {
    // Initialize global stats
    global_stats = BakeryStats();
    
    std::srand(std::time(0));
    
    int current_id = 0;
    
    // Create static objects
    DisplayObject nest("nest", 100, 80, 0, 1000);
    DisplayObject nest2("nest", 100, 80, 0, 1001);
    
    // Initialize nest states
    nest_states[1000] = NestState();
    nest_states[1001] = NestState();
    
    // Create egg display objects for nests
    std::vector<DisplayObject*> nest1_egg_objs;
    std::vector<DisplayObject*> nest2_egg_objs;
    
    for (int i = 0; i < 3; i++) {
        nest1_egg_objs.push_back(new DisplayObject("egg", egg_w, egg_h, 1, current_id++));
        nest2_egg_objs.push_back(new DisplayObject("egg", egg_w, egg_h, 1, current_id++));
    }
    
    nest_eggs[1000] = nest1_egg_objs;
    nest_eggs[1001] = nest2_egg_objs;
    
    // Position egg objects off-screen initially
    for (int i = 0; i < 3; i++) {
        nest1_egg_objs[i]->setPos(-100, -100);
        nest2_egg_objs[i]->setPos(-100, -100);
    }
    
    // Create barns and bakery
    DisplayObject barn1("barn", 100, 100, 0, current_id++);
    DisplayObject barn2("barn", 100, 100, 0, current_id++);
    DisplayObject bakery("bakery", 250, 250, 0, current_id++);
    
    // Create storage display items (for visual feedback)
    // Bakery storage area items
    DisplayObject* storage_eggs = new DisplayObject("egg", 30, 30, 1, current_id++);
    DisplayObject* storage_butter = new DisplayObject("butter", 30, 30, 1, current_id++);
    DisplayObject* storage_flour = new DisplayObject("flour", 30, 30, 1, current_id++);
    DisplayObject* storage_sugar = new DisplayObject("sugar", 30, 30, 1, current_id++);
    
    // Position storage items near bakery
    storage_eggs->setPos(BAKERY_X - 80, BAKERY_Y + 50);
    storage_butter->setPos(BAKERY_X - 40, BAKERY_Y + 50);
    storage_flour->setPos(BAKERY_X, BAKERY_Y + 50);
    storage_sugar->setPos(BAKERY_X + 40, BAKERY_Y + 50);
    
    // Position static objects
    nest.setPos(NEST1_X, NEST1_Y);
    nest2.setPos(NEST2_X, NEST2_Y);
    barn1.setPos(BARN1_X, BARN1_Y);
    barn2.setPos(BARN2_X, BARN2_Y);
    bakery.setPos(BAKERY_X, BAKERY_Y);
    
    // Update farm with static objects
    nest.updateFarm();
    nest2.updateFarm();
    barn1.updateFarm();
    barn2.updateFarm();
    bakery.updateFarm();
    storage_eggs->updateFarm();
    storage_butter->updateFarm();
    storage_flour->updateFarm();
    storage_sugar->updateFarm();
    
    // Start threads
    std::thread display_thread(display, std::ref(global_stats));
    std::thread oven(oven_thread);
    
    // 1 farmer starting away from trucks
    std::thread farmer1(farmer, 150, 250, current_id++);
    
    // 3 chickens starting at spread out positions
    std::thread chicken1(chicken, 250, 350, current_id++);
    std::thread chicken2(chicken, 400, 380, current_id++);
    std::thread chicken3(chicken, 550, 350, current_id++);
    
    // 2 cows positioned off to the side, static
    std::thread cow1(cow, 570, 300, current_id++);  // Far right side
    std::thread cow2(cow, 650, 300, current_id++);  // Far right side
    
    // 2 trucks - TOP truck for eggs/butter, BOTTOM truck for flour/sugar
    std::thread truck1(truck, BARN1_X, BARN1_Y, current_id++, true);   // Top truck - eggs/butter
    std::thread truck2(truck, BARN2_X, BARN2_Y, current_id++, false);  // Bottom truck - flour/sugar
    
    // 5 children starting in vertical line going UP from y=50
    std::thread child1(child, 800, 30, current_id++);   // Bottom of line (front)
    std::thread child2(child, 800, 200, current_id++);  // 2nd in line
    std::thread child3(child, 800, 400, current_id++);  // 3rd in line
    std::thread child4(child, 800, 500, current_id++);  // 4th in line
    std::thread child5(child, 800, 600, current_id++);  // Top of line (back)
    
    // Join threads
    display_thread.join();
    oven.join();
    chicken1.join();
    chicken2.join();
    chicken3.join();
    cow1.join();
    cow2.join();
    truck1.join();
    truck2.join();
    child1.join();
    child2.join();
    child3.join();
    child4.join();
    child5.join();
    farmer1.join();
}

void FarmLogic::start() {
    std::thread([]() {
       FarmLogic::run();
    }).detach();
}

