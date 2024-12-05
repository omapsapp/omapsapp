#pragma once

#include "base/math.hpp"

#include <initializer_list>
#include <iomanip>

namespace math
{
  template <typename T, unsigned Rows, unsigned Cols>
  struct Matrix
  {
    T m_data[Rows * Cols];

    Matrix() {}

    template <typename U>
    Matrix(Matrix<U, Rows, Cols> const & src)
    {
      for (size_t i = 0; i < Rows * Cols; ++i)
        m_data[i] = src.m_data[i];
    }

    Matrix(T * data)
    {
      copy(data, data + Rows * Cols, m_data);
    }

    Matrix(std::initializer_list<T> const & initList)
    {
      ASSERT(initList.size() == Rows * Cols, ());
      std::copy(initList.begin(), initList.end(), m_data);
    }

    template <typename U>
    Matrix & operator=(Matrix<U, Rows, Cols> const & src)
    {
      if ((void*)this != (void*)&src)
      {
        for (size_t i = 0; i < Rows * Cols; ++i)
          m_data[i] = src.m_data[i];
      }
      return *this;
    }

    T const & operator()(size_t row, size_t col) const
    {
      return m_data[row * Cols + col];
    }

    T & operator()(size_t row, size_t col)
    {
      return m_data[row * Cols + col];
    }

    template <typename U>
    bool operator == (Matrix<U, Rows, Cols> const & m) const
    {
      for (size_t i = 0; i < Rows; ++i)
        for (size_t j = 0; j < Cols; ++j)
          if (m_data[i * Cols + j] != m(i, j))
            return false;
      return true;
    }

    template <typename U>
        bool Equal(Matrix<U, Rows, Cols> const & m, T eps = 0.0001) const
    {
      for (size_t i = 0; i < Rows; ++i)
        for (size_t j = 0; j < Cols; ++j)
          if (Abs(m_data[i * Cols + j] - m(i, j)) > eps)
            return false;

      return true;
    }

    template <typename U>
    bool operator != (Matrix<U, Rows, Cols> const & m) const
    {
      return !(*this == m);
    }

    template <typename U>
    bool operator<(Matrix<U, Rows, Cols> const & m) const
    {
      for (size_t i = 0; i < Rows; ++i)
        for (size_t j = 0; j < Cols; ++j)
        {
          if (m_data[i * Cols + j] > m(i, j))
            return false;
          if (m_data[i * Cols + j] < m(i, j))
            return true;
        }

      /// All elements are equal
      return false;
    }
  };

  template <typename T>
  T Determinant(Matrix<T, 1, 1> const & m)
  {
    return m(0, 0);
  }

  template <typename T, unsigned M>
  T Determinant(Matrix<T, M, M> const & m)
  {
    T sign = 1;
    T res = 0;
    for (size_t i = 0; i < M; ++i, sign *= -1)
      res += sign * m(0, i) * Determinant(Splice(m, 0, i));
    return res;
  }

  template <typename T, unsigned Rows, unsigned Cols>
  Matrix<T, Rows - 1, Cols - 1> const Splice(Matrix<T, Rows, Cols> const & m, size_t Row, size_t Col)
  {
    Matrix<T, Rows - 1, Cols - 1> res;

    for (size_t i = 0; i < Row; ++i)
    {
      for (size_t j = 0; j < Col; ++j)
        res(i, j) = m(i, j);
      for (size_t j = Col + 1; j < Cols; ++j)
        res(i, j - 1)  = m(i, j);
    }

    for (size_t i = Row + 1; i < Rows; ++i)
    {
      for (size_t j = 0; j < Col; ++j)
        res(i - 1, j) = m(i, j);
      for (size_t j = Col + 1; j < Cols; ++j)
        res(i - 1, j - 1) = m(i, j);
    }

    return res;
  }

  template <typename T, unsigned M>
  Matrix<T, M, M> const Inverse(Matrix<T, M, M> const & m)
  {
    T det = Determinant(m);
    Matrix<T, M, M> res;

    for (size_t i = 0; i < M; ++i)
      for (size_t j = 0; j < M; ++j)
    {
      T sign = 1.0 - 2.0 * ((i + j) % 2);
      res(j, i) = sign * Determinant(Splice(m, i, j)) / det;
    }

    return res;
  }

  template <typename T, unsigned M>
  Matrix<T, M, M> const Identity()
  {
    Matrix<T, M, M> res;

    for (size_t i = 0; i < M; ++i)
      for (size_t j = 0; j < M; ++j)
        res(i, j) = 0;

    for (size_t i = 0; i < M; ++i)
      res(i, i) = 1;

    return res;
  }

  template <typename T, unsigned M>
  Matrix<T, M, M> const Zero()
  {
    Matrix<T, M, M> res;

    for (size_t i = 0; i < M; ++i)
      for (size_t j = 0; j < M; ++j)
        res(i, j) = 0;

    return res;
  }

  template <typename T, unsigned M, unsigned N, unsigned K>
  Matrix<T, M, K> operator*(Matrix<T, M, N> const & l, Matrix<T, N, K> const & r)
  {
    Matrix<T, M, K> res;
    for (size_t m = 0; m < M; ++m)
      for (size_t k = 0; k < K; ++k)
      {
        T sum = 0;
        for (size_t n = 0; n < N; ++n)
          sum += l(m, n) * r(n, k);
        res(m, k) = sum;
      }
    return res;
  }

  template <typename T, unsigned M, unsigned N> std::string DebugPrint(Matrix<T, M, N> const & m)
  {
    std::ostringstream ss;

    ss << ":" << std::endl;

    for (unsigned i = 0; i < M; ++i)
    {
      for (unsigned j = 0; j < N; ++j)
        ss << std::setfill(' ') << std::setw(10) << m(i, j) << " ";
      ss << std::endl;
    }

    return ss.str();
  }
}
