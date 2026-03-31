! Sample Fortran program demonstrating syntax highlighting
! This file exercises all TS captures from fortran-highlights.scm

module math_utils
    implicit none
    private
    public :: factorial, fibonacci, clamp_value

    ! Type declaration -> @property
    type :: vector3d
        real :: x, y, z
    end type vector3d

    integer, parameter :: MAX_SIZE = 1000
    real, parameter :: PI = 3.14159265358979

contains

    ! Function with result clause -> @keyword
    function factorial(n) result(res)
        integer, intent(in) :: n
        integer :: res
        integer :: i

        res = 1
        do i = 2, n
            res = res * i
        enddo
    end function factorial

    ! Recursive function
    recursive function fibonacci(n) result(fib)
        integer, intent(in) :: n
        integer :: fib

        if (n <= 1) then
            fib = n
        else
            fib = fibonacci(n - 1) + fibonacci(n - 2)
        endif
    end function fibonacci

    ! Subroutine with intent -> @property
    subroutine clamp_value(val, lo, hi)
        real, intent(inout) :: val
        real, intent(in) :: lo, hi

        if (val < lo) then
            val = lo
        elseif (val > hi) then
            val = hi
        endif
    end subroutine clamp_value

end module math_utils

program sample_program
    use math_utils, only: factorial, fibonacci, clamp_value
    implicit none

    ! Variable declarations -> @property for types
    integer :: i, n, result
    real :: x, total, temperature
    character(len=50) :: name
    logical :: is_valid
    complex :: z
    double precision :: big_val
    integer, dimension(10) :: arr
    real, allocatable :: dynamic_arr(:)
    integer, target :: tgt_val
    integer, pointer :: ptr_val

    ! I/O keywords -> @constant.builtin
    print *, "Enter a number:"
    read *, n

    ! Do loop -> @number.builtin
    total = 0.0
    do i = 1, n
        total = total + real(i)
    enddo

    ! Write with format
    write(*, '(A, F10.2)') "Sum: ", total
    write(*, '(A, I10)') "Factorial: ", factorial(n)

    ! While loop
    i = 1
    do while (i <= 10)
        arr(i) = i * i
        i = i + 1
    enddo

    ! If-elseif-else block
    temperature = 72.5
    if (temperature > 100.0) then
        name = "Hot"
    elseif (temperature > 70.0) then
        name = "Warm"
    elseif (temperature > 50.0) then
        name = "Cool"
    else
        name = "Cold"
    endif

    ! Boolean literals -> @function.special
    is_valid = .true.
    if (is_valid .and. n > 0) then
        print *, "Valid positive number"
    endif

    if (.not. is_valid .or. n == 0) then
        print *, "Invalid or zero"
    endif

    ! Operators -> @operator.word
    result = n + 10
    result = result - 5
    result = result * 2
    result = result / 3
    big_val = real(result) ** 2.5

    ! Comparison operators
    if (result == 42) print *, "Answer found"
    if (result /= 0) print *, "Non-zero"
    if (result < 100) print *, "Less than 100"
    if (result > 0) print *, "Positive"
    if (result <= 50) print *, "At most 50"
    if (result >= 10) print *, "At least 10"

    ! Select case -> @number.builtin
    select case (n)
    case (1)
        print *, "One"
    case (2:5)
        print *, "Two to five"
    case default
        print *, "Other"
    end select

    ! Call subroutine -> @number.builtin
    x = 150.0
    call clamp_value(x, 0.0, 100.0)
    write(*, '(A, F10.2)') "Clamped: ", x

    ! Allocate/deallocate -> @number.builtin
    allocate(dynamic_arr(n))
    do i = 1, n
        dynamic_arr(i) = sqrt(real(i))
    enddo

    ! File I/O -> @constant.builtin
    open(unit=10, file="output.txt", status="replace")
    do i = 1, n
        write(10, '(I5, F10.4)') i, dynamic_arr(i)
    enddo
    close(10)

    ! Inquire about file
    inquire(file="output.txt", exist=is_valid)
    if (is_valid) print *, "File exists"

    ! Cycle and exit in loops -> @number.builtin
    do i = 1, 20
        if (mod(i, 3) == 0) cycle
        if (i > 15) exit
        print *, i
    enddo

    ! Goto and continue -> @number.builtin
    goto 100
100 continue

    ! Return and stop -> @number.builtin
    deallocate(dynamic_arr)
    stop "Program completed"

end program sample_program
