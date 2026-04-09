package bench.ping_pong

// Scala Akka Ping-Pong Benchmark
// Classic actor framework comparison

import akka.actor.{Actor, ActorRef, ActorSystem, Props}
import scala.concurrent.duration._
import scala.concurrent.{Await, Promise}

case class Ping(value: Long)
case class Pong(value: Long)
case class Start(pong: ActorRef)
case class Done(count: Long)

class PingActor(promise: Promise[Long], messages: Long) extends Actor {
  var i = 0L
  val maxCount = messages
  var startTime = 0L
  var pongRef: ActorRef = null

  def receive = {
    case Start(pong) =>
      pongRef = pong
      startTime = System.nanoTime()
      i = 0
      pongRef ! Pong(i)

    case Ping(value) =>
      // Validate received value matches what was sent
      if (value != i) {
        System.err.println(s"Ping validation error: expected $i, got $value")
      }

      i += 1
      if (i < maxCount) {
        pongRef ! Pong(i)
      } else {
        val endTime = System.nanoTime()
        val elapsed = endTime - startTime
        promise.success(elapsed)
        context.system.terminate()
      }
  }
}

class PongActor(ping: ActorRef) extends Actor {
  var expected = 0L

  def receive = {
    case Pong(value) =>
      // Validate received value matches expected sequence
      if (value != expected) {
        System.err.println(s"Pong validation error: expected $expected, got $value")
      }
      expected += 1
      // Echo back the EXACT value received
      ping ! Ping(value)
  }
}

object PingPongBenchmark extends App {
  val messages = sys.env.get("BENCHMARK_MESSAGES").flatMap(s => scala.util.Try(s.toLong).toOption).getOrElse(100000L)

  println("=== Scala Akka Ping-Pong Benchmark ===")
  println(s"Messages: $messages\n")

  val system = ActorSystem("PingPong")
  val promise = Promise[Long]()

  val ping = system.actorOf(Props(classOf[PingActor], promise, messages: java.lang.Long))
  val pong = system.actorOf(Props(classOf[PongActor], ping))

  ping ! Start(pong)

  val elapsed = Await.result(promise.future, 120.seconds)
  val elapsedSec = elapsed.toDouble / 1e9
  val nsPerMsg = elapsed.toDouble / messages
  val throughput = messages / elapsedSec / 1e6

  println(f"ns/msg:         $nsPerMsg%.2f")
  println(f"Throughput:     $throughput%.2f M msg/sec")

  Await.result(system.whenTerminated, 10.seconds)
}
