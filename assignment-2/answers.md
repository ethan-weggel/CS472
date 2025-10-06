Ethan Weggel
erw74

Question 1: Time Travel Debugging
Your NTP client reports your clock is 30 seconds ahead, but you just synchronized yesterday. List three possible causes and how you'd investigate each one. Consider both technical issues (hardware, software, network) and real-world scenarios that could affect time synchronization.

    Please note, we have not covered some of these topics in class, but after doing some research, these are some of the causes and investigations I was able to come up with.

    **HARDWARE:**
    1) Local clock is drifting fast, perhaps due to a failing RTC/CMOS battery (which means my hardware clock would lose accuracy and would be reflected in software), or a VM I am running might run its clock too fast when my CPU is paused or throttled between tasks causing things to drift. In order to remedy this, I would investigate each issue separately. To see any discrepancies between the system and hardware clocks I would run `date -u` and `hwclock --show` to show major differences or jumps in what I would expect the time to show. In order to test the VM issue, I would temporarily disable a "host time sync" services and see this this slows the drift over a fixed interval. 

    **SOFTWARE:**
    2) One way that software might have caused this large drift after recent synchronization would be having multiple services all running and fighting one another that update, read or need access to the system time. This might cause some daemon disciplining issues and inject some delays due to differing steps or slews and NTP from different sources. Additionally, NTP or a servers network could be misconfigured severely. This includes but is not limited to things like syncing to a "falseticker" which is a bad server or either the sender or receiver's network has a firewall that blocks UDP over port 123 which would cause severe delays, especially if the user's time service was never notified. In order to investigate this, I would confirm that exactly one time service is running with a command like `systemctl list-units | egrep 'chronyd|ntpd|systemd-timesyncd'` and remove any extras from running simultaneously. I would also check my network and firewall from the client perspective and query servers using commands like `ntpdate -q pool.ntp.org` to see responses from multiple different sources. If timeouts or severe offsets occur, I'd fix the network or replace the faulty server.

    **REAL-WORLD:**
    3) Bad upstreams or "poisoned" NTP can happen when public/campus/cafe internet portals hijack or downgrade active NTP queries or responses. Also in similar scenarios, on-premises routers or access points might run their own SNTP giving the wrong time. If the Wifi in use seems like it could be at fault, I would try and cross check my NTP responses from mutliple sources such as using cellular data and running NTP query commands and would also check gross adjustment measures like HTTPS Date headers which could potentially catch tens of seconds worth of errors. I would also maybe use some route tracing technologies and see what servers I am actually hitting and make sure I am reading from a respectable pool (and manually switch if not). If available, I might also switch to a wired connection and rerun the same tests, maybe confirming the Wifi is suspect.

Question 2: Network Distance Detective Work
Test your NTP client with two different servers - one geographically close to you (like a national time service) and one farther away. Compare the round-trip delays you observe.

Based on your results, explain why the physical distance to an NTP server affects time synchronization quality. Why might you get a more accurate time sync from a "worse" time source that's closer to you rather than a "better" time source that's farther away? What does this tell us about distributed systems in general?

Include your actual test results and delay measurements in your answer.

    -   Server: time-a-b.nist.gov
        Location: Boulder, Colorado

        Received NTP response from time-a-b.nist.gov!
        --- Response Packet ---
        Leap Indicator: 0
        Version: 3
        Mode: 4
        Stratum: 1
        Poll: 13
        Precision: -29
        Reference ID: [0x4e495354] NIST
        Root Delay: 16
        Root Dispersion: 32
        Reference Time: 2025-10-04 20:27:44.000000 (Local Time)
        Original Time: 2025-10-04 20:29:27.818968 (Local Time)
        Receive Time: 2025-10-04 20:29:27.841858 (Local Time)
        Transmit Time: 2025-10-04 20:29:27.841859 (Local Time)

        === NTP Time Synchronization Results ===
        Server: time-a-b.nist.gov
        Server Time: 2025-10-04 20:29:27.841859 (Local Time)
        Client Time: 2025-10-04 20:29:27.866440 (Local Time)
        Round Trip Delay: 0.047471

        Time Offset: -0.000846 seconds
        Final Dispersion: 0.024346

        Your clock is running AHEAD by 0.845671ms
        Your estimated time error will be +/- 24.345646ms

    -   Server: time-a-g.nist.gov
        Location: Gaithersburg, Maryland
    
        Received NTP response from time-a-g.nist.gov!
        --- Response Packet ---
        Leap Indicator: 0
        Version: 3
        Mode: 4
        Stratum: 1
        Poll: 13
        Precision: -29
        Reference ID: [0x4e495354] NIST
        Root Delay: 16
        Root Dispersion: 32
        Reference Time: 2025-10-04 20:29:52.000000 (Local Time)
        Original Time: 2025-10-04 20:31:43.564003 (Local Time)
        Receive Time: 2025-10-04 20:31:43.571319 (Local Time)
        Transmit Time: 2025-10-04 20:31:43.571320 (Local Time)

        === NTP Time Synchronization Results ===
        Server: time-a-g.nist.gov
        Server Time: 2025-10-04 20:31:43.571320 (Local Time)
        Client Time: 2025-10-04 20:31:43.577224 (Local Time)
        Round Trip Delay: 0.013220

        Time Offset: 0.000706 seconds
        Final Dispersion: 0.007220

        Your clock is running BEHIND by 0.705719ms
        Your estimated time error will be +/- 7.220278ms

        Boulder, CO: 0.047471 sec
        Gaithersburg, MD: 0.013220 sec

        Just between these two time servers, one in Colorado and one in Maryland, we can see the difference in RTT by about 0.03 seconds. This is exemplary of the fact that it physically takes longer to travel to a server farther away with might increase variable delay AKA jitter; this directly can hurt our offset estimate. Additionally, if the forward path is quicker than the return path (or vice versa), the NTP formula for offset assumes symmetry since we are taking an average. If this is the case our offset will be biased and not very accurate. 

        A "worse" nearby source can beat a "better" far away source for the above reasons. Say a stratum-1 server way across the continent had between a 60-80 ms RTT, it could have worse jitter (i.e. less accurate offset) than say a stratum-2 server a few cities away with a 5-10 ms RTT. So in general, lower latency tends to win the battle for accuracy with all other variables remaining the same. 

        This tells us a few things about distributed systems in general. Since a distributed system is hosted on a network, we can now say with confidence that latency and variablility dominate accuracy of time related coordination. That said, proximity matters too. If a distributed system is primarily using close and quiet paths, this will beat an ideal system that is farther away, so physically setting up servers closer to one another means they will perform better. 


Question 3: Protocol Design Challenge
Imagine a simpler time protocol where a client just sends "What time is it?" to a server, and the server responds with "It's 2:30:15 PM".

Explain why this simple approach wouldn't work well for accurate time synchronization over a network. In your answer, discuss what problems network delay creates for time synchronization and why NTP needs to exchange multiple timestamps instead of just sending the current time. What additional information does having all four timestamps (T1, T2, T3, T4) provide that a simple request-response couldn't?

    - This more trivial protocol fails at accurate time synchronization over a network beacuse it ignores delay and variance in performance/availability. If one were to simply ask "What is the time?" and the server responds with "Hey! It's 2:30:15 PM", let's say that it takes about 50 ms for that response to reach the requester. That means by the time the requester can update their clock, 50 ms has already passed so the clock is never able to update accurately. Additionally, this 50 ms isn't even calculable and the request has no notion of a delay without additional information contained in the T1, T2, T3 and T4 timestamps. In that example, the network delay was 50 ms but it will vary between requests sinces queues fluctuate. Same with server response time; it could be instant or it could take a while to respond which could vary based on a multitude of reaons (e.g. traffic conditions, internal hardware specs, etc). 

    NTP's four timestamps fix this problem:
        - T1 - Client send
        - T2 - Server receive
        - T3 - Server transmit
        - T4 - Client receive

    - Round-trip delay: (T4 - T1) - (T3 - T2) gets rid of the server response time and gives us just the network transit time which we can now account for
    - Clock offset: ((T2 - T1) + (T3 - T4)) / 2 gets us the average of the forward and reverse legs of the trip to give a fairly good estimate of how ahead or behind our clock is