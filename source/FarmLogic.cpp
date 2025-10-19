#include "FarmLogic.h"
#include "displayobject.hpp"
#include <unistd.h>
#include <thread>
#include <cstdlib>
#include <ctime>
#include <chrono>



std::condition_variable display_cv;
//we're using display_mtx for erase, redisaply, and updateFarm, because they all need to be synced
std::mutex display_mtx;

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




bool out_of_bounds(DisplayObject &obj1, int x, int y){

    int left = (obj1.x+x)-(obj1.width/2);
    int right = (obj1.x+x)+(obj1.width/2);

    int up = (obj1.y+y)+(obj1.height/2);
    int down = (obj1.y+y)-(obj1.height/2);

    if (left<0 || right>800 || up>600 || down<0){
        return true;
    }
    
    return false;
}

void display(BakeryStats stats){
    // Do the redisplay action in a separate thread that loops: 
    // it should redisplay, sleep for a while, then repeat. Aim for at least 10 
    // frames per second (100ms sleep time).
    std::unique_lock<std::mutex> lk(display_mtx, std::defer_lock);

    while(true){
        auto sleepytime = std::chrono::system_clock::now()+ std::chrono::milliseconds(100);
        lk.lock();
        if(display_cv.wait_until(lk, sleepytime)==std::cv_status::timeout){
            DisplayObject::redisplay(stats);
        }
        lk.unlock();
      
    }
    
}


void chicken(int init_x, int init_y, int id){
    DisplayObject chicken("chicken", chicken_w, chicken_h, 2, id);
    chicken.setPos(init_x, init_y);

    //add this chicken for the first time
    std::unique_lock<std::mutex> lk(display_mtx);
    chicken.updateFarm();
    lk.unlock();

    int randomNumberX;
    int randomNumberY;
    while(true){
        auto sleepytime = std::chrono::system_clock::now()+ std::chrono::milliseconds(100);

        randomNumberX = (std::rand() % 11) - 5; // Generate a random number between -5 and 5
        randomNumberY = (std::rand() % 11) - 5; // Generate a random number between -5 and 5
        if(out_of_bounds(chicken, randomNumberX, randomNumberY)){
            //just switch both directions, quick fix
            randomNumberX*=-1;
            randomNumberY*=-1;
        }
        
        chicken.setPos(chicken.x + randomNumberX, chicken.y  + randomNumberY);

        lk.lock();
        if(display_cv.wait_until(lk, sleepytime)==std::cv_status::timeout){
            chicken.updateFarm();
        }
        lk.unlock();
        
    }
}

void cow(int init_x, int init_y, int id){

    DisplayObject cow("cow", cow_w, cow_h, 2, id);
    cow.setPos(init_x, init_y);

    //add this chicken for the first time
    std::unique_lock<std::mutex> lk(display_mtx);
    cow.updateFarm();
    lk.unlock();

    int randomNumberX;
    int randomNumberY;
    while(true){
        auto sleepytime = std::chrono::system_clock::now()+ std::chrono::milliseconds(100);

        randomNumberX = (std::rand() % 11) - 5; // Generate a random number between -5 and 5
        randomNumberY = (std::rand() % 11) - 5; // Generate a random number between -5 and 5
        
        if(out_of_bounds(cow, randomNumberX, randomNumberY)){
            //just switch both directions, quick fix
            randomNumberX*=-1;
            randomNumberY*=-1;
        }
        
        cow.setPos(cow.x + randomNumberX, cow.y  + randomNumberY);

        lk.lock();
        if(display_cv.wait_until(lk, sleepytime)==std::cv_status::timeout){
            cow.updateFarm();
        }
        lk.unlock();
        
    }
}

void child(int init_x, int init_y, int id){
    DisplayObject child("child", person_w, person_h, 2, id);
    child.setPos(init_x, init_y);

    //add this chicken for the first time
    std::unique_lock<std::mutex> lk(display_mtx);
    child.updateFarm();
    lk.unlock();

    int randomNumberX;
    int randomNumberY;
    while(true){
        auto sleepytime = std::chrono::system_clock::now()+ std::chrono::milliseconds(100);

        randomNumberX = (std::rand() % 11) - 5; // Generate a random number between -5 and 5
        randomNumberY = (std::rand() % 11) - 5; // Generate a random number between -5 and 5
        if(out_of_bounds(child, randomNumberX, randomNumberY)){
            //just switch both directions, quick fix
            randomNumberX*=-1;
            randomNumberY*=-1;
        }
        
        child.setPos(child.x + randomNumberX, child.y  + randomNumberY);

        lk.lock();
        if(display_cv.wait_until(lk, sleepytime)==std::cv_status::timeout){
            child.updateFarm();
        }
        lk.unlock();
        
    }

}

void farmer(int init_x, int init_y, int id){
   DisplayObject farmer("farmer", person_w, person_h, 2, id);
    farmer.setPos(init_x, init_y);

    std::unique_lock<std::mutex> lk(display_mtx);
    farmer.updateFarm();
    lk.unlock();

    int randomNumberX;
    int randomNumberY;
    while(true){
        auto sleepytime = std::chrono::system_clock::now()+ std::chrono::milliseconds(100);

        randomNumberX = (std::rand() % 11) - 5; // Generate a random number between -5 and 5
        randomNumberY = (std::rand() % 11) - 5; // Generate a random number between -5 and 5
        if(out_of_bounds(farmer, randomNumberX, randomNumberY)){
            //just switch both directions, quick fix
            randomNumberX*=-1;
            randomNumberY*=-1;
        }
        
        farmer.setPos(farmer.x + randomNumberX, farmer.y  + randomNumberY);

        lk.lock();
        if(display_cv.wait_until(lk, sleepytime)==std::cv_status::timeout){
            farmer.updateFarm();
        }
        lk.unlock();
        
    }


}


void truck(int init_x, int init_y, int id){
    DisplayObject truck("truck", truck_w, truck_h, 2, id);
    truck.setPos(init_x, init_y);

    //add this chicken for the first time
    std::unique_lock<std::mutex> lk(display_mtx);
    truck.updateFarm();
    lk.unlock();

    int randomNumberX;
    int randomNumberY;
    while(true){
        auto sleepytime = std::chrono::system_clock::now()+ std::chrono::milliseconds(100);

        randomNumberX = (std::rand() % 11) - 5; // Generate a random number between -5 and 5
        randomNumberY = (std::rand() % 11) - 5; // Generate a random number between -5 and 5

        if(out_of_bounds(truck, randomNumberX, randomNumberY)){
            //just switch both directions, quick fix
            randomNumberX*=-1;
            randomNumberY*=-1;
        }
        truck.setPos(truck.x + randomNumberX, truck.y  + randomNumberY);

        lk.lock();
        if(display_cv.wait_until(lk, sleepytime)==std::cv_status::timeout){
            truck.updateFarm();
        }
        lk.unlock();
        
    }

}

//later, for object intersections
bool intersects(DisplayObject &obj1, DisplayObject &obj2){
    // if (obj1.layer != obj2.layer){
    //     return false;
    // }
    // return true;
    return true;
}



void FarmLogic::run() {
    BakeryStats stats;

    std::srand(std::time(0));

    int current_id = 0;

    DisplayObject nest("nest", 100, 80, 0, current_id);
    DisplayObject nest2("nest", 100, 80, 0, current_id+=1);

    DisplayObject nest1eggs[3] = {
        DisplayObject("egg", egg_w, egg_h, 1, current_id+=1),
        DisplayObject("egg", egg_w, egg_h, 1, current_id+=1),
        DisplayObject("egg", egg_w, egg_h, 1, current_id+=1)
    };

    DisplayObject barn1("barn", 100, 100, 0, current_id+=1);
    DisplayObject barn2("barn", 100, 100, 0, current_id+=1);
    DisplayObject bakery("bakery", 250, 250, 0, current_id+=1);
    DisplayObject bakeryeggs[3] = {
        DisplayObject("egg", egg_w, egg_h, 1, current_id+=1),
        DisplayObject("egg", egg_w, egg_h, 1, current_id+=1),
        DisplayObject("egg", egg_w, egg_h, 1, current_id+=1)
    };
    DisplayObject bakeryflour[3] = {
        DisplayObject("flour", 20, 20, 1, current_id+=1),
        DisplayObject("flour", 20, 20, 1, current_id+=1),
        DisplayObject("flour", 20, 20, 1, current_id+=1)
    };
    DisplayObject bakerybutter[3] = {
        DisplayObject("butter", 20, 20, 1, current_id+=1),
        DisplayObject("butter", 20, 20, 1, current_id+=1),
        DisplayObject("butter", 20, 20, 1, current_id+=1)
    };
    DisplayObject bakerysugar[3] = {
        DisplayObject("sugar", 20, 20, 1, current_id+=1),
        DisplayObject("sugar", 20, 20, 1, current_id+=1),
        DisplayObject("sugar", 20, 20, 1, current_id+=1)
    };


    DisplayObject bakerycake[3] = {
        DisplayObject("cake", 20, 20, 1, current_id+=1),
        DisplayObject("cake", 20, 20, 1, current_id+=1),
        DisplayObject("cake", 20, 20, 1, current_id+=1)
    };


    nest.setPos(100, 500);
    nest2.setPos(700, 500);
    nest1eggs[0].setPos(90, 507);
    nest1eggs[1].setPos(100, 507);
    nest1eggs[2].setPos(110, 507);

    barn1.setPos(50, 50);
    barn2.setPos(50, 150);
    bakery.setPos(550, 150);
    bakeryeggs[0].setPos(510, 130);
    bakeryeggs[1].setPos(520, 130);
    bakeryeggs[2].setPos(530, 130);

    bakeryflour[0].setPos(500, 110);
    bakeryflour[1].setPos(520, 110);
    bakeryflour[2].setPos(540, 110);

    bakerysugar[0].setPos(500, 90);
    bakerysugar[1].setPos(520, 90);
    bakerysugar[2].setPos(540, 90);

    bakerybutter[0].setPos(500, 70);
    bakerybutter[1].setPos(520, 70);
    bakerybutter[2].setPos(540, 70);

    bakerycake[0].setPos(600, 200);
    bakerycake[1].setPos(620, 200);
    bakerycake[2].setPos(640, 200);

    nest.updateFarm();
    nest2.updateFarm();

    nest1eggs[0].updateFarm();
    nest1eggs[1].updateFarm();
    nest1eggs[2].updateFarm();

    barn1.updateFarm();
    barn2.updateFarm();
    bakery.updateFarm();
    bakeryeggs[0].updateFarm();
    bakeryeggs[1].updateFarm();
    bakeryeggs[2].updateFarm();

    bakeryflour[0].updateFarm();
    bakeryflour[1].updateFarm();
    bakeryflour[2].updateFarm();

    bakerybutter[0].updateFarm();
    bakerybutter[1].updateFarm();
    bakerybutter[2].updateFarm();

    bakerysugar[0].updateFarm();
    bakerysugar[1].updateFarm();
    bakerysugar[2].updateFarm();

    bakerycake[0].updateFarm();
    bakerycake[1].updateFarm();
    bakerycake[2].updateFarm();
    
    int randEggs = (std::rand() % 3);
    for(int i = 0; i < 3; i++) {
        if (i <= randEggs) {
            nest1eggs[i].updateFarm();
        } else {

            nest1eggs[i].erase();
        }
    }


    //threads for moving objects

    std::thread display_thread(display, stats);

    //1 farmer
    std::thread farmer1(farmer, 200,300, current_id+=1);

    //3 chickens
    std::thread chicken1(chicken, 300,400, current_id+=1);
    std::thread chicken2(chicken, 400, 300, current_id+=1);
    std::thread chicken3(chicken, 500, 300, current_id+=1);


    //2 cows
    std::thread cow1(cow, 200,200, current_id+=1);
    std::thread cow2(cow, 200,300, current_id+=1);


    //2 trucks
    std::thread truck1(truck, 300,200, current_id+=1);
    std::thread truck2(truck, 320,220, current_id+=1);

    //5 kids
    std::thread child1(child, 100,100, current_id+=1);
    std::thread child2(child, 110,100, current_id+=1);
    std::thread child3(child, 150,100, current_id+=1);
    std::thread child4(child, 620,100, current_id+=1);
    std::thread child5(child, 650,100, current_id+=1);


    display_thread.join();
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
    })
    .detach();
}