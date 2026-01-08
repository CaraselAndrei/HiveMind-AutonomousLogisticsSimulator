MAP GENERATION  

For map generation, an abstract interface is used, allowing the user to read the map from a file or generate one randoml. 

1. Procedural Strategy (ProceduralMapGenerator)  

a. Generation  

At the start of each iteration, the map is cleared (filled with .) and the destination/station lists are emptied. 

The Base ('B') is placed at a random position on the map. 

A number of walls equal to 40% of the total map area ((rows * cols) * 0.4) is calculated. 

These are placed randomly, marked with #, only on positions that are free (.). 

Charging stations (S) and clients (D) are placed on the remaining free random positions. 

Their coordinates are saved in the station_list and destinations lists to be used later by the agents. 

b. Validation  

After elements are placed, the isMapValid function is called. 

This uses the BFS (Breadth-First Search) algorithm starting from the Base to "flood fill" the map. 

The algorithm counts how many targets (D or S) it can physically reach, bypassing walls (#). 

Success condition: If the number of targets found by BFS is equal to the total number of stations + clients, the map is valid and the loop stops. 

Otherwise, "Invalid map (blocked)" is displayed, and the process resumes from zero. 

2. File-Based Strategy (FileMapLoader)  

Reading: Opens a specified text file ("harta_test"). 

Traversal: Reads the matrix character by character. 

Identification: As it traverses the file, it identifies special symbols: 

D: Adds coordinates to the destinations list. 

S: Adds coordinates to the stations list. 

B: Sets the base coordinates. 

If the file does not exist, it returns an empty map and displays an error. 

 

HIVEMIND ALGORITHM  

1. Allocation  

Agent Eligibility: An agent can receive packages only if: 

It is at the base. 

It is not in the DEAD or CHARGING state. 

It has battery over 20% (battery > maxBattery * 0.2). 

It has space in the inventory (areLoc()). 

Selection Logic: For each eligible agent, the system iterates through available packages and checks if they match using the canAgentTakePackage function. 

The selection criteria are: 

Feasibility: Estimated travel time (distance / speed + 2 margin ticks) must be less than the time remaining until the package expires. 

Profitability: Net Profit (Package Value - Agent Operating Cost) must be at least 50 credits. 

2. Departure Decision  

Agents do not leave immediately after receiving a package; they try to group multiple deliveries for efficiency. 

An agent leaves on a run only if one of the following conditions is met: 

Urgency: One of the loaded packages risks expiring if departure isn't immediate (the trebuieSaPleceUrgent method checks if the remaining time is close to the distance $+5$ ticks). 

Max Capacity: The agent has no room for other packages. 

Timeout: The agent has waited at the base for more than maxWaitTicks (15 ticks). 

3. Delivery  

Route Planning: The startDeliveryRoute method sets the target to the first package in the load list. 

The path is calculated using the $A^{*}$ algorithm (findPath) , taking into account whether the agent flies (ignores walls) or walks. 

Movement: In every tick (moveAgents), the agent advances on the current route, consuming battery. 

If it runs out of battery on the way, it dies (DEAD) and loses the packages. 

Actual Delivery: When the agent reaches the current package's destination coordinates: 

The package is marked DELIVERED (LIVRAT). 

The reward or penalty is calculated (if it arrived after the deadline). 

The package is removed from the agent's inventory. 

Chaining: If the agent still has packages in the inventory after a delivery, it immediately calls startDeliveryRoute for the next package in the list, calculating the route from the current position to the new client. 

Return: If the inventory is empty, the agent calculates its route back to the base (returnToBaseOrCharge) to wait for new orders or to charge 

 
