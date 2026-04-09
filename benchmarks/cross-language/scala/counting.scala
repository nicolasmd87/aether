package bench.counting

// Scala Akka Counting Actor Benchmark (Savina-style)
import akka.actor.{Actor, ActorRef, ActorSystem, Props}
import scala.concurrent.duration._
import scala.concurrent.{Await, Promise}

case object Increment
case class GetCount(replyTo: ActorRef)
case class Count(value: Long)

class CounterActor extends Actor {
  var count = 0L

  def receive = {
    case Increment =>
      count += 1
    case GetCount(replyTo) =>
      replyTo ! Count(count)
  }
}

class CollectorActor(promise: Promise[Long]) extends Actor {
  def receive = {
    case Count(value) =>
      promise.success(value)
      context.system.terminate()
  }
}

object CountingBenchmark extends App {
  val messages = sys.env.get("BENCHMARK_MESSAGES").flatMap(s => scala.util.Try(s.toLong).toOption).getOrElse(100000L)
  println("=== Scala Akka Counting Actor Benchmark ===")
  println(s"Messages: $messages\n")

  val system = ActorSystem("Counting")
  val promise = Promise[Long]()

  val counter = system.actorOf(Props[CounterActor]())
  val collector = system.actorOf(Props(classOf[CollectorActor], promise))

  val startTime = System.nanoTime()

  // Send all messages
  for (_ <- 0L until messages) {
    counter ! Increment
  }

  // Get count
  counter ! GetCount(collector)

  val count = Await.result(promise.future, 120.seconds)
  val endTime = System.nanoTime()

  val elapsed = (endTime - startTime).toDouble / 1e9

  if (count != messages) {
    println(s"VALIDATION FAILED: expected $messages, got $count")
  }

  val throughput = messages / elapsed / 1e6
  val nsPerMsg = elapsed * 1e9 / messages

  println(f"ns/msg:         $nsPerMsg%.2f")
  println(f"Throughput:     $throughput%.2f M msg/sec")

  Await.result(system.whenTerminated, 10.seconds)
}
