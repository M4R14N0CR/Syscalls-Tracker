<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
</head>
<body>
  <h1>System Call Tracker</h1>

  <h2>Overview</h2>
  <p>
    System calls are the fundamental mechanism that allows user-mode applications to communicate with the operating system kernel. This project is a system call tracker developed as part of the Operating Systems Principles course. It executes an external program and intercepts its system calls in real time, providing a detailed view of the program's interactions with the kernel in a POSIX environment.
  </p>

  <h2>Purpose</h2>
  <p>
    The tracker is designed to:
  </p>
  <ul>
    <li>Monitor and record system call invocations made by a target application.</li>
    <li>Display the name and description of each intercepted system call (when running in verbose mode).</li>
    <li>Demonstrate how user applications interact with the kernel for file management, process creation, device access, and other essential services.</li>
  </ul>


  <h2>Installation and Execution</h2>
  <ol>
    <li>
      <p><strong>Compile the Tracker</strong></p>
      <pre><code>gcc -std=c11 rastreador.c -o rastreador</code></pre>
    </li>
    <li>
      <p><strong>Run the Tracker</strong></p>
      <pre><code>rastreador [tracker options] Program [Program options]</code></pre>
      <p>
        In the tracker options, you can use -v or -V for different operating modes. "Program" is the executable whose system calls you want to trace, and "Program options" are the command-line arguments to be passed to that program (for example, executing <code>ls -l</code> where <code>ls</code> is the target and <code>-l</code> its argument).
      </p>
    </li>
  </ol>

