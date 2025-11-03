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
#include <random>
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


const int NEST1_X = 100, NEST1_Y = 500;
const int NEST2_X = 700, NEST2_Y = 500;
const int BARN1_X = 50, BARN1_Y = 150;   // Butter/eggs barn
const int BARN2_X = 50, BARN2_Y = 50;  // Flour/sugar barn
const int STORAGE_X = 550, STORAGE_Y = 150;
const int INTERSECTION_X = 300, INTERSECTION_Y = 150;
const int SHOP_X = 650, SHOP_Y = 150;




const int EGG_STORAGE_SHELF = STORAGE_Y -25;
const int BUTTER_STORAGE_SHELF= STORAGE_Y -50;
const int FLOUR_STORAGE_SHELF= STORAGE_Y -75;
const int SUGAR_STORAGE_SHELF= STORAGE_Y -100;


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

void update_position(int id, int x, int y, int width, int height, int layer) {
    std::lock_guard<std::mutex> lk(position_mtx);
    entity_positions[id] = {x, y, width, height, layer};
}

bool move_towards(DisplayObject &obj, int id, int target_x, int target_y, 
                  int speed, int width, int height, int layer) {
    int dx = 0, dy = 0;
    int dist_x = target_x - obj.x;
    int dist_y = target_y - obj.y;
    
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
    
    
    if (std::rand() % 100 < 20) {
        dy += (std::rand() % 3) - 1; 
    }
    
    if (out_of_bounds(obj, dx, dy)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lk(position_mtx);
    
    auto check_move = [&](int new_x, int new_y) -> bool {
        for (auto& [other_id, pos] : entity_positions) {
            if (other_id != id && pos.layer == layer) {
                if (check_collision(new_x, new_y, width, height,
                                  pos.x, pos.y, pos.width, pos.height)) {
                    return false;
                }
            }
        }
        return true;
    };
    
    int new_x = obj.x + dx;
    int new_y = obj.y + dy;
    
    if (check_move(new_x, new_y)) {
        obj.setPos(new_x, new_y);
        entity_positions[id] = {new_x, new_y, width, height, layer};
        return true;
    }
    
    if (dx != 0 && dy != 0) {
        if (std::rand() % 2 == 0) {
            // Try horizontal first
            if (check_move(obj.x + dx, obj.y)) {
                obj.setPos(obj.x + dx, obj.y);
                entity_positions[id] = {obj.x + dx, obj.y, width, height, layer};
                return true;
            }
            // Then vertical
            if (check_move(obj.x, obj.y + dy)) {
                obj.setPos(obj.x, obj.y + dy);
                entity_positions[id] = {obj.x, obj.y + dy, width, height, layer};
                return true;
            }
        } else {
            // Try vertical first
            if (check_move(obj.x, obj.y + dy)) {
                obj.setPos(obj.x, obj.y + dy);
                entity_positions[id] = {obj.x, obj.y + dy, width, height, layer};
                return true;
            }
            // Then horizontal
            if (check_move(obj.x + dx, obj.y)) {
                obj.setPos(obj.x + dx, obj.y);
                entity_positions[id] = {obj.x + dx, obj.y, width, height, layer};
                return true;
            }
        }
    }
    
    std::vector<std::pair<int, int>> dodge_moves;
    
    //random dodge directions and speeds
    for (int i = 0; i < 8; i++) {
        int rand_speed = speed + (std::rand() % 3) - 1;  
        int rand_x = 0, rand_y = 0;
        
        switch (std::rand() % 8) {
            case 0: rand_x = rand_speed; break;                      
            case 1: rand_x = -rand_speed; break;                     
            case 2: rand_y = rand_speed; break;                      
            case 3: rand_y = -rand_speed; break;                     
            case 4: rand_x = rand_speed; rand_y = rand_speed; break;   
            case 5: rand_x = -rand_speed; rand_y = rand_speed; break; 
            case 6: rand_x = rand_speed; rand_y = -rand_speed; break; 
            case 7: rand_x = -rand_speed; rand_y = -rand_speed; break; 
        }
        
        // vertical dodging is easier (mostly for chickens)
        if (std::rand() % 100 < 40) { 
            rand_y = (std::rand() % 2 == 0) ? speed * 2 : -speed * 2;
        }
        
        dodge_moves.push_back({rand_x, rand_y});
    }

    //super random movement
    std::shuffle(dodge_moves.begin(), dodge_moves.end(), std::mt19937(std::random_device{}()));    
    for (auto& [dodge_x, dodge_y] : dodge_moves) {
        if (!out_of_bounds(obj, dodge_x, dodge_y)) {
            int test_x = obj.x + dodge_x;
            int test_y = obj.y + dodge_y;
            
            if (check_move(test_x, test_y)) {
                if (std::rand() % 100 < 70) {
                    obj.setPos(test_x, test_y);
                    entity_positions[id] = {test_x, test_y, width, height, layer};
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

void chicken(int init_x, int init_y, int id, int starting_nest_idx) {
    DisplayObject chicken("chicken", chicken_w, chicken_h, 2, id);
    chicken.setPos(init_x, init_y);
    update_position(id, init_x, init_y, chicken_w, chicken_h, 2);
    
    {
        std::lock_guard<std::mutex> lk(display_mtx);
        chicken.updateFarm();
    }
    
    std::vector<int> nest_ids = {1000, 1001};
    

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> egg_dist(1, 3);

    int current_nest_idx = starting_nest_idx;
    while(true) {
        
        int target_nest_id = nest_ids[current_nest_idx];
        int nest_x = (target_nest_id == 1000) ? NEST1_X : NEST2_X;
        int nest_y = (target_nest_id == 1000) ? NEST1_Y : NEST2_Y;
        
        // Move toward nest 
        int attempts = 0;
        while ((abs(chicken.x - nest_x) > 20 || abs(chicken.y - nest_y) > 20) && attempts < 100) {
            move_towards(chicken, id, nest_x, nest_y, 4, chicken_w, chicken_h, 2);
            
            {
                std::lock_guard<std::mutex> disp_lk(display_mtx);
                chicken.updateFarm();
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50 + (id * 10))); 
            attempts++;
        }
        
        bool laid_eggs = false;
        int total_eggs_laid = 0;

        //we could have switched nests while other chickens are trying; recheck we're there
        if (abs(chicken.x - nest_x) <= 20 && abs(chicken.y - nest_y) <= 20) {
            {
                std::unique_lock<std::mutex> nest_lk(nest_mtx);
                
                auto result = nest_cv.wait_for(nest_lk, std::chrono::milliseconds(1000), [&] {
                    return !nest_states[target_nest_id].occupied || 
                        nest_states[target_nest_id].occupant_id == id;
                });
                
                if (result && nest_states[target_nest_id].egg_count < 3) {
                    nest_states[target_nest_id].occupied = true;
                    nest_states[target_nest_id].occupant_id = id;
                    
                    //how many eggs to lay (1-3)
                    int eggs_to_lay = egg_dist(gen);  // Instead of 1 + (std::rand() % 3)

                    //don't exceed nest capacity of 3
                    int available_space = 3 - nest_states[target_nest_id].egg_count;
                    eggs_to_lay = std::min(eggs_to_lay, available_space);
                    
                    for (int i = 0; i < eggs_to_lay; i++) {
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
                        
                        total_eggs_laid++;
                    }
                    
                    laid_eggs = true;
                    
                    nest_states[target_nest_id].occupied = false;
                    nest_states[target_nest_id].occupant_id = -1;
                }
                
                nest_cv.notify_all();
            }
        }
        //switch to the other nest 
        current_nest_idx = (current_nest_idx + 1) % nest_ids.size();
        
        std::this_thread::yield();
    }
}

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
        
        
        int approach_y = nest_y - 60; 
        
        //move toward nest from below
        int attempts = 0;
        while ((abs(farmer.x - nest_x) > 30 || abs(farmer.y - approach_y) > 30) && attempts < 300) {
            move_towards(farmer, id, nest_x, approach_y, 5, person_w, person_h, 2);
            {
                std::lock_guard<std::mutex> disp_lk(display_mtx);
                farmer.updateFarm();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            attempts++;
        }
        
        //move up to the nest for collection
        while ((abs(farmer.x - nest_x) > 30 || abs(farmer.y - nest_y) > 30) && attempts < 350) {
            move_towards(farmer, id, nest_x, nest_y, 5, person_w, person_h, 2);
            {
                std::lock_guard<std::mutex> disp_lk(display_mtx);
                farmer.updateFarm();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            attempts++;
        }
        
        {
            std::unique_lock<std::mutex> nest_lk(nest_mtx);
            bool got_eggs = nest_cv.wait_for(nest_lk, std::chrono::seconds(3), [&] {
                return !nest_states[target_nest_id].occupied &&
                       nest_states[target_nest_id].egg_count > 0;
            });

            if (got_eggs) {
                int eggs_collected = nest_states[target_nest_id].egg_count;
                nest_states[target_nest_id].egg_count = 0;
                nest_states[target_nest_id].eggs_by_chicken.clear();

                {
                    std::lock_guard<std::mutex> disp_lk(display_mtx);
                    for (int i = 0; i < eggs_collected; i++) {
                        if (i < nest_eggs[target_nest_id].size()) {
                            nest_eggs[target_nest_id][i]->setPos(-100, -100);
                            nest_eggs[target_nest_id][i]->updateFarm();
                        }
                    }
                }
                {
                    int barn_target_y = BARN1_Y + 20;
                    attempts = 0;
                    while ((abs(farmer.x - BARN1_X) > 30 || abs(farmer.y - barn_target_y) > 30) && attempts < 200) {
                        move_towards(farmer, id, BARN1_X, barn_target_y, 5, person_w, person_h, 2);
                        {
                            std::lock_guard<std::mutex> disp_lk(display_mtx);
                            farmer.updateFarm();
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        attempts++;
                    }

                    // wait for access to barn (truck may be using it)
                    std::unique_lock<std::mutex> barn_lk(barn_mtx);
                    

                    barn1_state.eggs += eggs_collected;
                    barn_cv.notify_all();
                }

    
                nest_cv.notify_all();
            } 
            else {
                // waited too long — switch nests
                current_nest = (current_nest + 1) % nest_ids.size();
                nest_cv.notify_all();
                continue;
            }
        }

        current_nest = (current_nest + 1) % nest_ids.size();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

// void truck(int init_x, int init_y, int id, bool is_barn1) {
//     DisplayObject truck("truck", truck_w, truck_h, 2, id);
//     truck.setPos(init_x, init_y);
//     update_position(id, init_x, init_y, truck_w, truck_h, 2);
    
//     {
//         std::lock_guard<std::mutex> lk(display_mtx);
//         truck.updateFarm();
//     }
    
//     int barn_x = BARN1_X;
//     int barn_y = is_barn1 ? BARN1_Y : BARN2_Y;
    
//     struct Cargo {
//         int eggs = 0;
//         int butter = 0;
//         int flour = 0;
//         int sugar = 0;
//     } cargo;
    
//     while(true) {
//         // Move to barn
//         while (abs(truck.x - barn_x) > 10 || abs(truck.y - barn_y) > 10) {
//             if (move_towards(truck, id, barn_x, barn_y, 5, truck_w, truck_h, 2)) {
//                 {
//                     std::lock_guard<std::mutex> disp_lk(display_mtx);
//                     truck.updateFarm();
//                 }
//             }
//             std::this_thread::sleep_for(std::chrono::milliseconds(100));
//         }
        
//         // Load at barn
//         {
//             std::unique_lock<std::mutex> barn_lk(barn_mtx);
            
//             if (is_barn1) {
//                 barn_cv.wait(barn_lk, [&] { return barn1_state.eggs >= 3; });
//                 cargo.eggs = std::min(3, barn1_state.eggs);
//                 cargo.butter = 3;
//                 barn1_state.eggs -= cargo.eggs;
//                 barn1_state.butter += 3;
                
//                 {
//                     std::lock_guard<std::mutex> stats_lk(stats_mtx);
//                     global_stats.butter_produced += 3;
//                 }
//             } else {
//                 cargo.flour = 3;
//                 cargo.sugar = 3;
//                 barn2_state.flour += 3;
//                 barn2_state.sugar += 3;
                
//                 {
//                     std::lock_guard<std::mutex> stats_lk(stats_mtx);
//                     global_stats.flour_produced += 3;
//                     global_stats.sugar_produced += 3;
//                 }
//             }
//         }
        
//         // Path to intersection: Move horizontally
//         while (abs(truck.x - INTERSECTION_X) > 10) {
//             if (move_towards(truck, id, INTERSECTION_X, truck.y, 5, truck_w, truck_h, 2)) {
//                 {
//                     std::lock_guard<std::mutex> disp_lk(display_mtx);
//                     truck.updateFarm();
//                 }
//             }
//             std::this_thread::sleep_for(std::chrono::milliseconds(100));
//         }
        
//         // Request intersection access
//         {
//             std::unique_lock<std::mutex> int_lk(intersection_mtx);
//             truck_queue.push(id);
            
//             intersection_cv.wait(int_lk, [&] {
//                 return !intersection_occupied && 
//                        !truck_queue.empty() && 
//                        truck_queue.front() == id;
//             });
            
//             intersection_occupied = true;
//             truck_queue.pop();
//         }
        
//         // Move through intersection to bakery
//         while (abs(truck.y - STORAGE_Y) > 10) {
//             if (move_towards(truck, id, truck.x, STORAGE_Y, 5, truck_w, truck_h, 2)) {
//                 {
//                     std::lock_guard<std::mutex> disp_lk(display_mtx);
//                     truck.updateFarm();
//                 }
//             }
//             std::this_thread::sleep_for(std::chrono::milliseconds(100));
//         }
        
//         while (abs(truck.x - STORAGE_X) > 10) {
//             if (move_towards(truck, id, STORAGE_X, truck.y, 5, truck_w, truck_h, 2)) {
//                 {
//                     std::lock_guard<std::mutex> disp_lk(display_mtx);
//                     truck.updateFarm();
//                 }
//             }
//             std::this_thread::sleep_for(std::chrono::milliseconds(100));
//         }
        
//         // Release intersection
//         {
//             std::lock_guard<std::mutex> int_lk(intersection_mtx);
//             intersection_occupied = false;
//             intersection_cv.notify_all();
//         }
        
//         // Unload at bakery
//         {
//             std::unique_lock<std::mutex> bakery_lk(bakery_mtx);
            
//             bakery_cv.wait(bakery_lk, [&] {
//                 return (bakery_state.eggs + cargo.eggs <= 6) &&
//                        (bakery_state.butter + cargo.butter <= 6) &&
//                        (bakery_state.flour + cargo.flour <= 6) &&
//                        (bakery_state.sugar + cargo.sugar <= 6);
//             });
            
//             bakery_state.eggs += cargo.eggs;
//             bakery_state.butter += cargo.butter;
//             bakery_state.flour += cargo.flour;
//             bakery_state.sugar += cargo.sugar;
            
//             cargo = {};
//             oven_cv.notify_all();
//         }
        
//         // Return path: Back through intersection
//         while (abs(truck.x - INTERSECTION_X) > 10) {
//             if (move_towards(truck, id, INTERSECTION_X, truck.y, 5, truck_w, truck_h, 2)) {
//                 {
//                     std::lock_guard<std::mutex> disp_lk(display_mtx);
//                     truck.updateFarm();
//                 }
//             }
//             std::this_thread::sleep_for(std::chrono::milliseconds(100));
//         }
        
//         // Move back to barn lane
//         int return_y = is_barn1 ? BARN1_Y : BARN2_Y;
//         while (abs(truck.y - return_y) > 10) {
//             if (move_towards(truck, id, truck.x, return_y, 5, truck_w, truck_h, 2)) {
//                 {
//                     std::lock_guard<std::mutex> disp_lk(display_mtx);
//                     truck.updateFarm();
//                 }
//             }
//             std::this_thread::sleep_for(std::chrono::milliseconds(100));
//         }
        
//         // Move to barn
//         while (abs(truck.x - barn_x) > 10) {
//             if (move_towards(truck, id, barn_x, truck.y, 5, truck_w, truck_h, 2)) {
//                 {
//                     std::lock_guard<std::mutex> disp_lk(display_mtx);
//                     truck.updateFarm();
//                 }
//             }
//             std::this_thread::sleep_for(std::chrono::milliseconds(100));
//         }
//     }
// }

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

    while (true) {
        while (abs(truck.x - barn_x) > 90 || abs(truck.y - barn_y) > 90) {
            if (move_towards(truck, id, barn_x, barn_y, 5, truck_w, truck_h, 2)) {
                std::cout<<"MEOW";
                std::lock_guard<std::mutex> disp_lk(display_mtx);
                truck.updateFarm();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        {
            std::unique_lock<std::mutex> barn_lk(barn_mtx);

            if (is_barn1) {
                // Wait until enough eggs are available
                barn_cv.wait(barn_lk, [&] { return barn1_state.eggs >= 3; });

                cargo.eggs = 3;
                cargo.butter = 3;  // butter is produced per trip
                barn1_state.eggs -= 3;

                {
                    std::lock_guard<std::mutex> stats_lk(stats_mtx);
                    global_stats.eggs_used += 3;
                    global_stats.butter_produced += 3;
                }
            } else {
                // Truck 2 can always be loaded
                cargo.flour = 3;
                cargo.sugar = 3;

                {
                    std::lock_guard<std::mutex> stats_lk(stats_mtx);
                    global_stats.flour_produced += 3;
                    global_stats.sugar_produced += 3;
                }
            }
        }

        while (abs(truck.x - INTERSECTION_X) > 10) {
            if (move_towards(truck, id, INTERSECTION_X, truck.y, 5, truck_w, truck_h, 2)) {
                std::lock_guard<std::mutex> disp_lk(display_mtx);
                truck.updateFarm();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Request intersection access
        {
            std::unique_lock<std::mutex> int_lk(intersection_mtx);
            truck_queue.push(id);

            intersection_cv.wait(int_lk, [&] {
                return !intersection_occupied && !truck_queue.empty() && truck_queue.front() == id;
            });

            intersection_occupied = true;
            truck_queue.pop();
        }

        // move through intersection to storage
        while (abs(truck.y - STORAGE_Y) > 10) {
            if (move_towards(truck, id, truck.x, STORAGE_Y, 5, truck_w, truck_h, 2)) {
                std::lock_guard<std::mutex> disp_lk(display_mtx);
                truck.updateFarm();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        while (abs(truck.x - STORAGE_X) > 10) {
            if (move_towards(truck, id, STORAGE_X, truck.y, 5, truck_w, truck_h, 2)) {
                std::lock_guard<std::mutex> disp_lk(display_mtx);
                truck.updateFarm();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Release intersection
        {
            std::lock_guard<std::mutex> int_lk(intersection_mtx);
            intersection_occupied = false;
            intersection_cv.notify_all();
        }

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
            bakery_cv.notify_all();  // wake trucks waiting to unload
        }

        while (abs(truck.x - INTERSECTION_X) > 10) {
            if (move_towards(truck, id, INTERSECTION_X, truck.y, 5, truck_w, truck_h, 2)) {
                std::lock_guard<std::mutex> disp_lk(display_mtx);
                truck.updateFarm();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        int return_y = is_barn1 ? BARN1_Y : BARN2_Y;
        while (abs(truck.y - return_y) > 10) {
            if (move_towards(truck, id, truck.x, return_y, 5, truck_w, truck_h, 2)) {
                std::lock_guard<std::mutex> disp_lk(display_mtx);
                truck.updateFarm();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        while (abs(truck.x - barn_x) > 10) {
            if (move_towards(truck, id, barn_x, truck.y, 5, truck_w, truck_h, 2)) {
                std::lock_guard<std::mutex> disp_lk(display_mtx);
                truck.updateFarm();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}



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

void child(int init_x, int init_y, int id) {
    DisplayObject child("child", person_w, person_h, 2, id);
    
    static std::mutex line_position_mtx;
    static int next_in_line = 0;
    int my_queue_position;
    
    {
        std::lock_guard<std::mutex> lk(line_position_mtx);
        my_queue_position = next_in_line++;
    }
    
    int line_x = 775; 
    int base_line_y = 60;  
    
    int line_y = base_line_y + (my_queue_position * (person_h + 10));

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

            //do i need this?
            shop_cv.notify_all();
            
            shop_cv.wait(shop_lk, [&] {
                return current_shopper == -1 && 
                       !child_queue.empty() && 
                       child_queue.front() == id;
            });
            
            child_queue.pop();
            current_shopper = id;

        }
        
        // Move to shop
        while (abs(child.x - SHOP_X) > 5 || abs(child.y - SHOP_Y) > 5) {
            if (move_towards(child, id, SHOP_X, SHOP_Y, 4, person_w, person_h, 2)){
                {
                    std::lock_guard<std::mutex> disp_lk(display_mtx);
                    child.updateFarm();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }


        // Buy cakes
        int want_cakes = (std::rand() % 6) + 1;
        {
            std::unique_lock<std::mutex> bakery_lk(bakery_mtx);
            bakery_cv.wait(bakery_lk, [&] {
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
    global_stats = BakeryStats();
    
    std::srand(std::time(0));
    
    int current_id = 0;
    
    DisplayObject nest("nest", 100, 80, 0, 1000);
    DisplayObject nest2("nest", 100, 80, 0, 1001);
    
    nest_states[1000] = NestState();
    nest_states[1001] = NestState();
    
    std::vector<DisplayObject*> nest1_egg_objs;
    std::vector<DisplayObject*> nest2_egg_objs;
    
    for (int i = 0; i < 3; i++) {
        nest1_egg_objs.push_back(new DisplayObject("egg", egg_w, egg_h, 1, current_id++));
        nest2_egg_objs.push_back(new DisplayObject("egg", egg_w, egg_h, 1, current_id++));
    }
    
    nest_eggs[1000] = nest1_egg_objs;
    nest_eggs[1001] = nest2_egg_objs;
    
    for (int i = 0; i < 3; i++) {
        nest1_egg_objs[i]->setPos(-100, -100);
        nest2_egg_objs[i]->setPos(-100, -100);
    }

    DisplayObject barn1("barn", 100, 100, 0, current_id++);
    DisplayObject barn2("barn", 100, 100, 0, current_id++);
    DisplayObject bakery("bakery", 250, 250, 0, current_id++);

    // const int STORAGE_X = 550, STORAGE_Y = 150;

    DisplayObject* storage_eggs = new DisplayObject("egg", 30, 30, 1, current_id++);
    DisplayObject* storage_butter = new DisplayObject("butter", 30, 30, 1, current_id++);
    DisplayObject* storage_flour = new DisplayObject("flour", 30, 30, 1, current_id++);
    DisplayObject* storage_sugar = new DisplayObject("sugar", 30, 30, 1, current_id++);
    
    storage_eggs->setPos(STORAGE_X , EGG_STORAGE_SHELF);
    storage_butter->setPos(STORAGE_X, BUTTER_STORAGE_SHELF);
    storage_flour->setPos(STORAGE_X, FLOUR_STORAGE_SHELF);
    storage_sugar->setPos(STORAGE_X , SUGAR_STORAGE_SHELF);
    
    nest.setPos(NEST1_X, NEST1_Y);
    nest2.setPos(NEST2_X, NEST2_Y);
    barn1.setPos(BARN1_X, BARN1_Y);
    barn2.setPos(BARN2_X, BARN2_Y);
    bakery.setPos(STORAGE_X, STORAGE_Y);
    
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
    
    // 1 farmer 
    std::thread farmer1(farmer, 150, 250, current_id++);
    
    // 3 chickens 
    std::thread chicken3(chicken, 400, 540, current_id++, 1);

    std::thread chicken2(chicken, 550, 550, current_id++,1);
    std::thread chicken1(chicken, 250, 550, current_id++, 0);

    // 2 cows
    std::thread cow1(cow, 570, 300, current_id++);  
    std::thread cow2(cow, 650, 300, current_id++);  
    

    //2 truck
    std::thread truck1(truck, BARN1_X+90, BARN1_Y, current_id++, true);   //  eggs/butter
    std::thread truck2(truck, BARN2_X+90, BARN2_Y, current_id++, false);  // flour/sugar
    
    // 5 kids
    std::thread child1(child, 800, 30, current_id++);  
    std::thread child2(child, 800, 200, current_id++); 
    std::thread child3(child, 800, 400, current_id++); 
    std::thread child4(child, 800, 500, current_id++); 
    std::thread child5(child, 800, 600, current_id++); 
    
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