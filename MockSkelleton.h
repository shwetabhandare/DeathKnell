#pragma once
#include "Skelleton.h"
#include "g2log.hpp"
#include <atomic>
#include <thread>
#include <memory>
#ifdef LR_DEBUG

class MockSkelleton : public Skelleton {
public:

   /**
    * This is a "Router" object intended to be an end point of a number of
    *   "Dealer" objects
    * @param binding
    *   The binding is stored, but not bound till Initialize is called
    */
   explicit MockSkelleton(const std::string& binding) : Skelleton(binding), mRepeating(false), mRepeaterThread(nullptr), mEmptyReplies(false) {
   }

   /**
    * Deconstructor
    *   Since the base class is virtual, this does nothing.  The context will be cleaned up
    * in the "real" class. 
    */
   virtual ~MockSkelleton() {
      if (mRepeating) {
         EndListendAndRepeat();
      }
      if (nullptr == mRepeaterThread) {
         mRepeaterThread.reset(nullptr);
      }
   }

   /**
    * Bind the socket, will return false in several abnormal cases. 
    * @return 
    */
   bool Initialize() override {
      mContext = zctx_new();
      if (!mContext) {
         LOG(WARNING) << "queue error " << zmq_strerror(zmq_errno());
         return false;
      }
      mFace = zsocket_new(mContext, ZMQ_ROUTER);
      if (!mFace) {
         LOG(WARNING) << "queue error " << zmq_strerror(zmq_errno());
         zctx_destroy(&mContext);
         mFace = NULL;
         return false;
      }
      if (zsocket_bind(mFace, mBinding.c_str()) < 0) {
         LOG(WARNING) << "queue error " << zmq_strerror(zmq_errno());
         zctx_destroy(&mContext);
         mFace = NULL;
         return false;
      }
      return true;
   }

   /**
    * A simple "read" echo back what you heard with " reply" function
    *   built for testing within its own thread
    */
   void RepeatMessages() {
      while (mRepeating.load() && !zctx_interrupted) {
         if (zsocket_poll(mFace, 100)) {
            zmsg_t* msg = zmsg_recv(mFace);
            std::vector<zframe_t*> envelopes; // A place to put all the zmq routing info
            /*  The last part of a message is the actual data send, before that 
             * what are referred to as envelopes.  Each dealer packages whatever 
             * it sends in a new envelope with a return address.  Also, additional 
             * routing information (eg a message id) may also be an envelope added
             * by the software.  These need to be preseved in order and put back on 
             * the reply.
             */
            while (zmsg_size(msg) > 1) { // the last part of a message is the actual message
               envelopes.push_back(zmsg_pop(msg));
            }
            // Last one is the actual message
            char* msgChar = zmsg_popstr(msg);
            std::string reply = msgChar;
            free(msgChar);
            if (mReplyMessage.empty() && !mEmptyReplies) {
               reply += " reply";
            } else {
               reply = mReplyMessage;
            }
            /* We are responsible for cleaning the message up, pop has removed those
             *  frame references.
             */
            zmsg_destroy(&msg);

            msg = zmsg_new();
            /**
             * Put the old envelopes back on, in order.  Their return addresses will
             * be used to route back to the sender.
             */
            for (auto envelope : envelopes) {
               zmsg_add(msg, envelope);
            }
            /**
             * Add our reply to the end
             */
            zmsg_addstr(msg, reply.c_str());
            /* Send will take care of the memory associated with msg
             */
            zmsg_send(&msg, mFace);
         }
      }

   }

   /**
    * Spin up a simple read/echo thread
    */
   void BeginListenAndRepeat() {

      mRepeating.store(true);
      mRepeaterThread.reset(new std::thread(&MockSkelleton::RepeatMessages, this));
   }

   /**
    * Spin the read/echo thread down
    */
   void EndListendAndRepeat() {
      mRepeating.store(false);

      if (nullptr != mRepeaterThread) {
         mRepeaterThread->join();
         mRepeaterThread.reset(nullptr);
      }
   }

   std::string mReplyMessage;
   bool mEmptyReplies;
private:
   std::atomic<bool> mRepeating;
   std::unique_ptr<std::thread> mRepeaterThread;
   
};
#endif

