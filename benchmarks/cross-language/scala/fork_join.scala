// Scala Akka Fork-Join Benchmark (Savina-style)
import akka.actor.{Actor, ActorRef, ActorSystem, Props}
import scala.concurrent.duration._
import scala.concurrent.{Await, Promise}

case class Work(value: Int)
case object Done
case class Result(processed: Int)

class WorkerActor(parent: ActorRef) extends Actor {
  var processed = 0

  def receive = {
    case Work(_) =>
      processed += 1
    case Done =>
      parent ! Result(processed)
  }
}

class CoordinatorActor(promise: Promise[Int], numWorkers: Int, messagesPerWorker: Int) extends Actor {
  var workers: Array[ActorRef] = null
  var resultsReceived = 0
  var totalProcessed = 0
  var startTime = 0L

  override def preStart(): Unit = {
    // Create workers
    workers = Array.tabulate(numWorkers)(i =>
      context.actorOf(Props(classOf[WorkerActor], self), s"worker$i")
    )

    startTime = System.nanoTime()

    // Send messages round-robin
    val total = numWorkers * messagesPerWorker
    for (i <- 0 until total) {
      workers(i % numWorkers) ! Work(i)
    }

    // Signal workers to report
    workers.foreach(_ ! Done)
  }

  def receive = {
    case Result(processed) =>
      totalProcessed += processed
      resultsReceived += 1
      if (resultsReceived == numWorkers) {
        promise.success(totalProcessed)
        context.system.terminate()
      }
  }
}

object ForkJoinBenchmark extends App {
  val numWorkers = 8
  val messagesPerWorker = 1000000
  val total = numWorkers * messagesPerWorker

  println("=== Scala Akka Fork-Join Throughput Benchmark ===")
  println(s"Workers: $numWorkers, Messages: $total\n")

  val system = ActorSystem("ForkJoin")
  val promise = Promise[Int]()

  val startTime = System.nanoTime()
  val coordinator = system.actorOf(Props(classOf[CoordinatorActor], promise, numWorkers, messagesPerWorker))

  val totalProcessed = Await.result(promise.future, 120.seconds)
  val endTime = System.nanoTime()

  val elapsed = (endTime - startTime).toDouble / 1e9

  if (totalProcessed != total) {
    println(s"VALIDATION FAILED: expected $total, got $totalProcessed")
  }

  val throughput = total / elapsed / 1e6
  val cyclesPerMsg = elapsed * 3e9 / total

  println(f"Cycles/msg:     $cyclesPerMsg%.2f")
  println(f"Throughput:     $throughput%.2f M msg/sec")

  Await.result(system.whenTerminated, 10.seconds)
}
