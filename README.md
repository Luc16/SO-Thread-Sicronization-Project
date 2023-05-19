[![pt-br](https://img.shields.io/badge/LEIAME-PT--BR-A3BE8C.svg?style=for-the-badge)](https://github.com/pesader/webapp-factory/blob/master/README.ptbr.md)

# Searchers, Inserters, and Deleters

An interactive visualization of the "The search-insert-delete problem" proposed
by the [Little Book of Semaphores](https://greenteapress.com/wp/semaphores/),
powered by a custom-made TUI engine. It was developed as an assignment on
multithreaded programming for the 2023.1 Operating Systems class at Unicamp.

<details>
<summary>Contents</summary>
<!-- vim-markdown-toc GFM -->

* [Demo](#demo)
* [The problem](#the-problem)
* [The solution](#the-solution)
    * [Lightswitch](#lightswitch)
    * [Turnstile](#turnstile)
* [Building and running](#building-and-running)
* [References](#references)
* [License](#license)

<!-- vim-markdown-toc -->
</details>

## Demo

## The problem

Here's the problem, as described in the [Little Book of
Semaphores](https://greenteapress.com/wp/semaphores/):

> Three kinds of threads share access to a singly-linked list:
> searchers, inserters and deleters. Searchers merely examine the list;
> hence they can execute concurrently with each other. Inserters add
> new items to the end of the list; insertions must be mutually ex-
> clusive to preclude two inserters from inserting new items at about
> the same time. However, one insert can proceed in parallel with
> any number of searches. Finally, deleters remove items from any-
> where in the list. At most one deleter process can access the list at
> a time, and deletion must also be mutually exclusive with searches
> and insertions.

## The solution

Since there's no starvation-proof solution for this problem, the best we can do
is prioritizing whichever operation is known to happen the least. This takes
care of avoiding deadlocks in most practical cases. It is still possible to
guarantee mutual exclusion without deadlocks, though! To do that we used the
following synchronization primitives (besides mutex locks and semaphores):

### Lightswitch

The main idea of the lightswitch is that "first in turns on the lights, last
out turns off the lights". When deleters are not prioritized, inserters and
searchers use their own lightswitch (`inserterSwitch` and `searcherSwitch`) to
signal that deleters can enter the list when "the light is off".

### Turnstile

When the operations of wait and post of a semaphore are called in quick succession
its called a <b>Turnstile</b>. This pattern is beneficial when prioritizing `deleters`
to stop the activities of both `searchers` and `inserters` upon their arrival, 
while allowing unrestricted movement for the latter two in other cases.

## Building and running

This program depends on:

- make
- g++

Install them on your system with:

```bash
sudo apt install build-essential # on Debian-based distros
sudo dnf install make gcc-c++    # on Fedora-based distros
sudo pacman -S make gcc          # on Arch-based distros
```

Then, compile and run the program with:

```bash
make
```

The program has the following keybindings:

| Key | Action |
|-----|--------|
| `i` | insert random letter after random target |
| `s` | search random target |
| `d` | delete random target |
| `I` | insert input letter after input target |
| `S` | search input target |
| `D` | delete input target |
| `+` | increase speed |
| `-` | decrease speed |
| `Space` | toggle priority mode |
| `Esc` | quit the program |

## References

- [Little Book of Semaphores](https://greenteapress.com/wp/semaphores/)

## License

This work is licensed under the terms of the GPLv3.
