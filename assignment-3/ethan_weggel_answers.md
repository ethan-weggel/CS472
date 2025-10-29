**Ethan Weggel**
**(erw74)**

# Crypto Echo - Concept Questions

## Instructions

Answer the following questions to demonstrate your understanding of the networking concepts and design decisions in this assignment. Your answers should be thoughtful and demonstrate understanding of the underlying principles, not just surface-level descriptions.

**Submission Requirements:**
- Answer all 5 questions
- Each answer should be 1-2 paragraphs (150-300 words)
- Use specific examples from the assignment when applicable
- Explain the "why" behind design decisions, not just the "what"

---

## Question 1: TCP vs UDP - Why Stateful Communication Matters

**Question:**
This assignment requires you to use TCP instead of UDP. Explain in detail **why TCP is necessary** for this encrypted communication application. In your answer, address:
- What specific features of TCP are essential for maintaining encrypted sessions?
- What would break or become problematic if we used UDP instead?
- How does the stateful nature of TCP support the key exchange and encrypted communication?

**Hint:** Think about what happens to the encryption keys during a session and what TCP guarantees that UDP doesn't.

   TCP is essential to use in this case because our encrypted channel relies on a stable, shared session context. After the key exchange PDU, each side of the connection must maintain their keys and remain synchronized in that session state while messages flow. If either side were to lose state of the other, we might end up encrypting/decrypting with mismatched keys, encrypting when we are not supposed to or decrypting when we are not supposed to. Mistakes like these are much easier to perform when using UDP because if a segment was lost or reordered, we'd desynchronize our encryption state which would scramble our messages or we might even communicate the incorrect key. With TCP's connection-oriented model, we get a reliable, continuous byte stream that allows for in-order delivery.

   With UDP, each datagram is independent meaning we'd have to reinvent a lot of things to keep our high-level mechanisms working. Things such as session ID's, ordering, loss recovery, retransmission and key re-exchange on loss would cease to exist unless we deliberately remade these mechanisms within a far more complex application layer. The fix for this with UDP would be to retransmit keys and session information every time but this is costly and insecure. Additionally large PDU's might fragment with UDP and might increase the chance of partial/missing data. 

---

## Question 2: Protocol Data Unit (PDU) Structure Design

**Question:**
Our protocol uses a fixed-structure PDU with a header containing `msg_type`, `direction`, and `payload_len` fields, followed by a variable-length payload. Explain **why we designed the protocol this way** instead of simpler alternatives. Consider:
- Why not just send raw text strings like "ENCRYPT:Hello World"?
- What advantages does the binary PDU structure provide?
- How does this structure make the protocol more robust and extensible?
- What would be the challenges of parsing messages without a structured header?

**Hint:** Think about different types of data (text, binary, encrypted bytes) and how the receiver knows what it's receiving.

   We use a fixed-structure PDU with our format being [msg_type | direction | payload_len | payload...] because it makes parsing deterministic and standardized, we support binary data and we enable evolution of our protocol by having a standard building block of our fixed size PDU header. Raw strings like `ENCRYPT:Hello World` are human-readable which is nice, but in general it makes them brittle. When we make them raw strings we start to assume we have text-only payloads (which eliminates the ability to expand later) and we depend on delimiters which may also occur within our payload. The PDU's we define carry lengths and types explicitly so receivers can allocate buffers safely, know exactly how many bytes to read next, and we know which function to call next to handle the corresponding bytes read (and casted). Knowing exactly what byte lies where in the header also allows us to parse strictly rather than dynamically which speeds up our algorithms on the client and server sides.

   Our current framing also makes our protocol extensible in the sense we can introduce new message type `(msg_type)` values (e.g. REKEY, PING, ERROR, etc) without changing our parsing logic -- we essentially have deployed a design pattern or just really strategic modularity to an otherwise un-object-oriented langauge. Having this **fixed size header** also allows us to know when to start parsing our **dynamically sized payload** rather than look for a delimiter which might show up in some hashing function results or algorithms -- thus also limiting which algorithms or encodings we might be able to use going forward. 

---

## Question 3: The Payload Length Field

**Question:**
TCP is a **stream-oriented protocol** (not message-oriented), yet our PDU includes a `payload_len` field. Explain **why this field is critical** even though TCP delivers all data reliably. In your answer, address:
- How does TCP's stream nature differ from message boundaries?
- What problem does the `payload_len` field solve?
- What would happen if we removed this field and just relied on TCP?
- How does `recv()` work with respect to message boundaries?

**Hint:** Consider what happens when multiple PDUs are sent in rapid succession, or when a large PDU arrives in multiple `recv()` calls.

   Even though TCP is reliable through its stream-orientedness rather than focusing on definitive messages, it being a stream introduces its own problems. `recv()` returns whatever bytes are currently available which could be less than, equal to or more than one logically defined message. For instance, if we send two PDU's back to back the receiver might get both headers together, a header split across calls or a header plus partial payload. The `payload_len` field tells the parser how many bytes to pull from the stream to complete the **current PDU** before starting the next.

   If we were to remove this field we'd need need to resort back to our fragile delimiters to detect our message boundaries which can break under encrypted/binary paryloads and is very error prone during fragmentation. With the field in mind our read loop becomes very simple and well-defined: read the fixed-sized ehader, validate fields, loop until we've read in `payload_len` bytes and continue. This avoids the issue of buffer overflows, accidental truncation or even blending one message into another. So even though TCP will deliver all of our data, knowing where to start reading and stop reading under real network conditions is paramount to success with a stream based protocol.

---

## Question 4: Key Exchange Protocol and Session State

**Question:**
The key exchange must happen **before** any encrypted messages can be sent, and keys are **session-specific** (new keys for each connection). Explain **why this design is important** and what problems it solves. Address:
- Why can't we just use pre-shared keys (hardcoded in both client and server)?
- What security or practical benefits come from generating new keys per session?
- What happens if the connection drops after key exchange? Why is this significant?
- How does this relate to the choice of TCP over UDP?

**Hint:** Think about what "session state" means and how it relates to the TCP connection lifecycle.

   Having a per-session or really per-connection key exchange allows us to guarantee forward secrecy, key isolation and simplicty in our implementation. Harding a pre-shared key is a risk because with one leak of binary data and many users are compromised using the same protocol, rotating pre-defined keys is generally harder to deploy in a real-world scenario in circumstances like this and reusing the same key might also reveal some traffic correlations. Generating keys per session limits the impact of that key to just that session which is generally better for everyone involved.

   Based on our implementation, if we have a connection and it drops, that session state is now gone which is exactly what we want to have happen. Then when we reconnect and a client requests a new session key, the server gets a new session key as well. This eliminates stale traffic and encrypting over a similar or the same key. When we do it this way we are also correlating the lifecycle of our key exchange to the lifecycle of TCP in our protocol. When we connect we get a well-defined beginning with a three-way handshake and an end when we disconnect. If we were to switch to UDP we;d need explicit session ID's or timeouts or logic in place to re-key bewteen client-server sessions to just approximate this lifecycle, but it would be hard to make it exact; TCP gives us clear scoping of "as long as our socket lives our keys can live".

---

## Question 5: The Direction Field in the PDU Header

**Question:**
Every PDU includes a `direction` field (DIR_REQUEST or DIR_RESPONSE), even though the client and server already know their roles. Some might argue this field is redundant. Explain **why we include it anyway** and what value it provides. Consider:
- How does this field aid in debugging and protocol validation?
- What would happen if you accidentally swapped request/response handling code?
- How does it make the protocol more self-documenting?
- Could this protocol be extended to peer-to-peer communication? How does the direction field help?

**Hint:** Think about protocol clarity, error detection, and future extensibility beyond simple client-server.

   When we include our direction (REQUEST/RESPONSE), we make the protocol we are using self-describing. In logs and dumps of our data, seeing a direction gives us an immediate sanity check in the sense that if the server emits REQUEST or the client receive a REQUEST when it expected a RESPONSE, we can instantly flag it as a logic error. This helps us debug but also make sure messages are being sent in the right direction at a high level. This is a big help in protocol validation as well which we can see a bit of in our `print_msg_info()` function.

   ```
   if ((mode == SERVER_MODE && pdu->direction == DIR_REQUEST) ||
      (mode == CLIENT_MODE && pdu->direction == DIR_RESPONSE)) {

      if(decrypt_string(key, msg_data, msg->payload, pdu->payload_len) > 0) {
            msg_data[pdu->payload_len] = '\0'; // Null-terminate
            printf("  Payload (decrypted): \"%s\"\n", msg_data);
      } else {
            printf("  Payload: Decryption error\n");
      }

   }
   ```

   Here we can see we are testing for CLIENT_MODE, SERVER_MODE and DIR_REQUEST and DIR_RESPONSE. This means we are able to decrypt messages accordingly provided the client sends requests and the server sends responses. However, even if we were to switch this up and make it so we could have peer-to-peer connections and have bidirectional messages, then we no longer have to assume who is sending and who is receiving -- it is explicitly stated and allows for upscaling of our application. In other words, having this field reduces our coupling of our roles to their functionality and would make it easier to reuse the same headers in peer-to-peer expansion.

---

## Extra Credit Documentation

**Hash Function Choice**

   ```
   static uint8_t compute_hash(const char *message, size_t len) {
      uint8_t hash = 0;
      for (size_t i = 0; i < len; i++) {
         hash ^= message[i];
         hash = (hash << 1) | (hash >> 7);
      }
      return hash;
   }
   ```

   Here I used the provided example code because I figured it was a good way to represent hashing without going too far in the simplistic direction or too far in complexity. I appreciate that I can tell that we are rotating bits and older bits continue to influence future steps. I also like the efficiency of logical operations like XOR-ing bits. This is a quick and clean example of a hashing function that is easy to inject into my existing code.

**Example Interactions**



**Signature Verification Failure**

**Tampering Prevention**

---



