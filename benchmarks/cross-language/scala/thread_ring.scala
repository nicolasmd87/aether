package bench.thread_ring

// Scala Akka Thread Ring Benchmark (Savina-style)
import akka.actor.{Actor, ActorRef, ActorSystem, Props}
import scala.concurrent.duration._
import scala.concurrent.{Await, Promise}

case class SetNext(next: ActorRef)
case class Token(hops: Int)
case class RingDone(received: Int)

class RingNodeActor(parent: ActorRef) extends Actor {
  var next: ActorRef = null
  var received = 0

  def receive = {
    case SetNext(n) =>
      next = n
    case Token(0) =>
      received += 1
      parent ! RingDone(received)
    case Token(hops) =>
      received += 1
      next ! Token(hops - 1)
  }
}

class ParentActor(promise: Promise[Int], ringSize: Int, numHops: Int) extends Actor {
  var nodes: Array[ActorRef] = null

  override def preStart(): Unit = {
    // Create ring nodes
    nodes = Array.tabulate(ringSize)(i =>
      context.actorOf(Props(classOf[RingNodeActor], self), s"node$i")
    )

    // Link them in a ring
    for (i <- 0 until ringSize) {
      nodes(i) ! SetNext(nodes((i + 1) % ringSize))
    }

    // Inject token
    nodes(0) ! Token(numHops)
  }

  def receive = {
    case RingDone(received) =>
      promise.success(received)
      context.system.terminate()
  }
}

object ThreadRingBenchmark extends App {
  val ringSize = 100
  val numHops = sys.env.get("BENCHMARK_MESSAGES").flatMap(s => scala.util.Try(s.toInt).toOption).getOrElse(100000)
  println("=== Scala Akka Thread Ring Benchmark ===")
  println(s"Ring size: $ringSize, Hops: $numHops\n")

  val system = ActorSystem("ThreadRing")
  val promise = Promise[Int]()

  val startTime = System.nanoTime()

  val parent = system.actorOf(Props(classOf[ParentActor], promise, ringSize, numHops))

  val received = Await.result(promise.future, 120.seconds)
  val endTime = System.nanoTime()

  val elapsed = (endTime - startTime).toDouble / 1e9
  val totalMessages = numHops + 1

  if (received != totalMessages) {
    println(s"VALIDATION FAILED: expected $totalMessages, got $received")
  }

  val throughput = totalMessages / elapsed / 1e6
  val nsPerMsg = elapsed * 1e9 / totalMessages

  println(f"ns/msg:         $nsPerMsg%.2f")
  println(f"Throughput:     $throughput%.2f M msg/sec")

  Await.result(system.whenTerminated, 10.seconds)
}
