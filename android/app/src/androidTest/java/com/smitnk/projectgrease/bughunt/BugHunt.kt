package com.smitnk.projectgrease.bughunt

/**
 * Bug-hunt environment tests (.github/workflows/bughunt.yml): long fuzzing, the 30-minute soak, the
 * frame-time budget and the process-death pair. The PR sweep excludes them (-e notAnnotation).
 */
@Retention(AnnotationRetention.RUNTIME)
@Target(AnnotationTarget.CLASS)
annotation class BugHunt
