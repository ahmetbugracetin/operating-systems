#include "crossroad.h"
#include "hw2_output.h"
#include <pthread.h>
#include "monitor.h"
#include <queue>
#include <vector>
#include <climits>
using namespace std;

// TODO: Define your global intersection state variables here
// e.g., dynamically allocated arrays for lane queues
class A: public Monitor {   // inherit from Monitor
    int* h_pri;
    int* v_pri;
    vector<vector<queue<int>>> queues; //for FIFO logic    // condition varibles
    vector<Condition*> h_conditions;
    vector<Condition*> v_conditions;
    int arrived_time = 0;
    vector<vector<int>> in_critical_section; // 0 for h, 1 for v
    int check = 0;
    int check1 = 0;
    int check2 = 0;
    vector<vector<queue<int>>> time_arrived;
public:
    A(int h_lanes, int v_lanes, int* h_pri, int* v_pri){   // pass "this" to cv constructors
        this->h_pri = h_pri;
        this->v_pri = v_pri;
        for(int i= 0;i<h_lanes; i++){
            h_conditions.push_back(new Condition(this));
        }
        for(int i= 0;i<v_lanes; i++){
            v_conditions.push_back(new Condition(this));
        }
        queues.push_back(vector<queue<int>>());  
        queues.push_back(vector<queue<int>>());
        for(int i= 0;i<v_lanes; i++){
            queues[1].push_back(queue<int>());
        }
        for(int i= 0;i<h_lanes; i++){
            queues[0].push_back(queue<int>());
        }
        time_arrived.push_back(vector<queue<int>>());  
        time_arrived.push_back(vector<queue<int>>());
        for(int i= 0;i<v_lanes; i++){
            time_arrived[1].push_back(queue<int>());
        }
        for(int i= 0;i<h_lanes; i++){
            time_arrived[0].push_back(queue<int>());
        }
        in_critical_section.push_back(vector<int>(h_lanes,0));
        in_critical_section.push_back(vector<int>(v_lanes,0));
    }
    void arrive_crossroad(int car_id, Direction dir, int lane) {
        __synchronized__;
        hw2_write_output(car_id, ET_ARRIVE, dir, lane);
        // implement your monitor method. lock is already acquired
        // thanks to macro call above.
        if(dir == 0) {arrived_time++;queues[dir][lane].push(car_id); time_arrived[dir][lane].push(arrived_time);}
        if(dir == 1) {arrived_time++;queues[dir][lane].push(car_id); time_arrived[dir][lane].push(arrived_time);}
        
        while (true) { // I need to wait for an event
            check1 = 0;//is there anyone coming from the crossroads have higher priority than me
            check = 0;//is there anyone in the critical section coming from the any lane in the crossroads.
            check2= 0; // if our priorities same look out arrival time
            for(int i = 0; i<in_critical_section[!dir].size(); i++){
                if(in_critical_section[!dir][i]==1) {check = 1; break;}
            }
            for(int i = 0; i<in_critical_section[!dir].size(); i++){
                if(dir==1 && h_pri[i]>v_pri[lane] && !queues[0][i].empty()) {check1 =1; break;}
                if(dir==0 && v_pri[i]>h_pri[lane] && !queues[1][i].empty()) {check1 =1; break;}
            }
            for(int i = 0; i<in_critical_section[!dir].size(); i++){
                if(dir==1 && h_pri[i]==v_pri[lane] && !queues[0][i].empty() && time_arrived[0][i].front()<time_arrived[dir][lane].front()) {check2 =1; break;}
                if(dir==0 && v_pri[i]==h_pri[lane] && !queues[1][i].empty() && time_arrived[1][i].front()<time_arrived[dir][lane].front()) {check2 =1; break;}
            }
            if((in_critical_section[dir][lane]==0 && check == 0 && !queues[dir][lane].empty() && queues[dir][lane].front() == car_id && check1 == 0 && check2==0)){
                break;
            }
            if(dir == 0) h_conditions[lane]->wait();
            if(dir == 1) v_conditions[lane]->wait();

        }
        hw2_write_output(car_id, ET_ENTER, dir, lane);
        queues[dir][lane].pop();
        time_arrived[dir][lane].pop();
        in_critical_section[dir][lane]++;

    }  // no need to unlock here, destructor of macro variable does it

    void exit_crossroad(int car_id, Direction dir, int lane) {
        __synchronized__;
        
        in_critical_section[dir][lane]--;
        hw2_write_output(car_id, ET_EXIT, dir, lane);
        

        for(int i = 0; i<in_critical_section[0].size();i++){
            h_conditions[i]->notifyAll();
        }
        for(int i = 0; i<in_critical_section[1].size();i++){
            v_conditions[i]->notifyAll();
        }
        // method1() // !!!! you should not do that.
    }
}; A *monitor;

void initialize_crossroad(int h_lanes, int v_lanes, int* h_pri, int* v_pri) {
    // TODO: Dynamically allocate your state arrays based on the lane counts
    // TODO: Initialize your mutexes and condition variables
    monitor = new A(h_lanes,v_lanes, h_pri, v_pri);
}

void arrive_crossroad(int car_id, Direction dir, int lane) {
    // 1. Log arrival
    // 2. Wait condition (Strict FIFO + Group Mutual Exclusion)
    monitor->arrive_crossroad(car_id,dir,lane);
    // 3. Log entry
}

void exit_crossroad(int car_id, Direction dir, int lane) {
    // 1. Update state
    monitor->exit_crossroad(car_id,dir,lane);
    // 2. Log exit
    // 3. Signal waiting threads
}