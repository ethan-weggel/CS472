Ethan Weggel
erw74

Question 1: Time Travel Debugging
Your NTP client reports your clock is 30 seconds ahead, but you just synchronized yesterday. List three possible causes and how you'd investigate each one. Consider both technical issues (hardware, software, network) and real-world scenarios that could affect time synchronization.

Question 2: Network Distance Detective Work
Test your NTP client with two different servers - one geographically close to you (like a national time service) and one farther away. Compare the round-trip delays you observe.

Based on your results, explain why the physical distance to an NTP server affects time synchronization quality. Why might you get a more accurate time sync from a "worse" time source that's closer to you rather than a "better" time source that's farther away? What does this tell us about distributed systems in general?

Include your actual test results and delay measurements in your answer.

Question 3: Protocol Design Challenge
Imagine a simpler time protocol where a client just sends "What time is it?" to a server, and the server responds with "It's 2:30:15 PM".

Explain why this simple approach wouldn't work well for accurate time synchronization over a network. In your answer, discuss what problems network delay creates for time synchronization and why NTP needs to exchange multiple timestamps instead of just sending the current time. What additional information does having all four timestamps (T1, T2, T3, T4) provide that a simple request-response couldn't?