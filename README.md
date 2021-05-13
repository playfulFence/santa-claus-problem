# santa-claus-problem

###### (no Windows, sry)

Santa Claus sleeps in his workshop at the North Pole and can only be woken up

1. At a time when all his reindeers are back from summer vacation.
2. Some of his elves have a problem making toys and need help.  

The elves are waiting in front of the workshop. If there's at least 3 waiting elves, the first 3 from the queue will enter the workshop. 
The other elves who need help must wait in front of the workshop. 

At the moment when the last reindeer comes from vacation, elves can be "postponed". 
Santa puts a "Christmas - Closed" sign on the workshop door and goes to hitch reindeers.  

All the elves waiting in front of the workshop immediately go on vacation. 

The other elves go on vacation when they need help from Santa and find that the workshop is closed.

    ---------------------------------------DETAILED DESCRIPTION------------------------------------
      FORMAT : ./santa_problem NE NR TE TR
    |-----------------------------------------------------------------------------------------|
    |• NE - number of elves (0 < NE < 1000)                                                   | 
    |-----------------------------------------------------------------------------------------|
    |• NR - number of reindeers (0 < NR < 1000)                                               |
    |-----------------------------------------------------------------------------------------|
    |• TE - maximal time(ms), which elf working independently (0 <= TE <= 1000)               | 
    |-----------------------------------------------------------------------------------------|
    |• TR - maximal time(ms), for which reindeer will return home from vacation               |
    |       (0 <= TR <= 1000)                                                                 |
    |____________________________________________________________________by_playfulFence______|
